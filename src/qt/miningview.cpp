#include "miningview.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "platformstyle.h"
#include "univalue.h"
#include "util.h"
#include "rpc/client.h"
#include "rpc/server.h"
#include "rpc/protocol.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QThread>

const int MiningWorker::MINING_INTERVAL;

// MiningWorker method implementations
void MiningWorker::calibrateMaxTries()
{
    // Run a test mining operation with 10,000 tries and measure time
    const uint64_t TEST_TRIES = 10000;
    QElapsedTimer timer;
    timer.start();

    JSONRPCRequest request;
    request.strMethod = "generate";
    request.params = UniValue(UniValue::VARR);
    request.params.push_back(1);        // Generate 1 block
    request.params.push_back(TEST_TRIES); // Use test number of tries

    try {
        tableRPC.execute(request);
    } catch (const UniValue& objError) {
        // Ignore errors during calibration
    }

    // Calculate how many tries we can do in MINING_INTERVAL milliseconds
    int elapsed = timer.elapsed();
    if (elapsed > 0) {
        m_maxTriesPerInterval = (TEST_TRIES * MINING_INTERVAL) / elapsed;
        m_maxTriesPerInterval = m_maxTriesPerInterval * 9 / 10;  // Safety margin
        
        // Calculate hash rate (hashes per second)
        double hashRate = (m_maxTriesPerInterval * 1000.0) / MINING_INTERVAL;
        Q_EMIT hashRateChanged(hashRate);
        
        LogPrintf("Mining calibration: %d tries took %.2f seconds\n", 
                  TEST_TRIES, elapsed/1000.0);
        LogPrintf("Mining calibration: will use %lu tries per %d ms interval\n", 
                  m_maxTriesPerInterval, MINING_INTERVAL);
    }
}

void MiningWorker::startMining()
{
    if (m_mining)
        return;

    m_mining = true;
    
    // Calibrate maxTries before starting
    calibrateMaxTries();
    
    Q_EMIT miningStarted();
    
    // Start periodic mining
    m_miningTimer->start(MINING_INTERVAL);
    mineBlock(); // Mine first block immediately
}

void MiningWorker::mineBlock()
{
    if (!m_mining)
        return;

    JSONRPCRequest request;
    request.strMethod = "generate";
    request.params = UniValue(UniValue::VARR);
    request.params.push_back(1);     // Generate 1 block
    request.params.push_back(m_maxTriesPerInterval);  // Use calibrated value

    try {
        UniValue result = tableRPC.execute(request);
    } catch (const UniValue& objError) {
        QString errorMessage = QString::fromStdString(find_value(objError, "message").get_str());
        Q_EMIT miningFailed(errorMessage);
        stopMining();
    }
}

void MiningWorker::stopMining()
{
    if (!m_mining)
        return;

    m_mining = false;
    if (m_miningTimer) {
        m_miningTimer->stop();
    }
    Q_EMIT hashRateChanged(0);  // Set hash rate to 0
    Q_EMIT miningStopped();
}

// MiningView method implementations
MiningView::MiningView(const PlatformStyle *platformStyle, QWidget *parent) :
    QWidget(parent),
    clientModel(nullptr),
    walletModel(nullptr),
    miningWorker(nullptr),
    miningThread(nullptr)
{
    QVBoxLayout *mainLayout = new QVBoxLayout();
    setLayout(mainLayout);

    QHBoxLayout *controlsLayout = new QHBoxLayout();
    
    startMiningButton = new QPushButton(tr("Start Mining"));
    stopMiningButton = new QPushButton(tr("Stop Mining"));
    stopMiningButton->setEnabled(false);
    hashRateLabel = new QLabel(tr("Hash Rate: 0 H/s"));
    networkHashRateLabel = new QLabel(tr("Network Hash Rate: 0 H/s"));
    
    controlsLayout->addWidget(startMiningButton);
    controlsLayout->addWidget(stopMiningButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(hashRateLabel);
    controlsLayout->addWidget(networkHashRateLabel);
    
    mainLayout->addLayout(controlsLayout);
    mainLayout->addStretch();

    // Connect button signals to slots
    connect(startMiningButton, &QPushButton::clicked, this, &MiningView::startMining);
    connect(stopMiningButton, &QPushButton::clicked, this, &MiningView::stopMining);
    
    // Update network hash rate when the view loads
    updateNetworkHashRate();

    // Create the mining worker and thread
    miningWorker = new MiningWorker();
    miningThread = new QThread(this);
    miningWorker->moveToThread(miningThread);

    connect(miningThread, &QThread::finished, miningWorker, &QObject::deleteLater);
    connect(this, &MiningView::startMiningSignal, miningWorker, &MiningWorker::startMining);
    connect(this, &MiningView::stopMiningSignal, miningWorker, &MiningWorker::stopMining);
    connect(miningWorker, &MiningWorker::miningStopped, this, &MiningView::onMiningStopped);
    connect(miningWorker, &MiningWorker::miningFailed, this, &MiningView::onMiningFailed);
    connect(miningWorker, &MiningWorker::hashRateChanged, this, &MiningView::onHashRateChanged);

    miningThread->start();
}

MiningView::~MiningView()
{
    miningThread->quit();
    miningThread->wait();
}

void MiningView::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    
    if (model) {
        // Connect to blockchain signals
        connect(model, &ClientModel::numBlocksChanged, this, &MiningView::updateNetworkHashRate);
    }
}

void MiningView::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
}

void MiningView::startMining()
{
    if (!clientModel)
        return;

    startMiningButton->setEnabled(false);
    stopMiningButton->setEnabled(true);

    Q_EMIT startMiningSignal();
}

void MiningView::stopMining()
{
    if (!clientModel)
        return;

    Q_EMIT stopMiningSignal();
}

void MiningView::onMiningStopped()
{
    startMiningButton->setEnabled(true);
    stopMiningButton->setEnabled(false);
    hashRateLabel->setText(tr("Mining stopped"));
}

void MiningView::onMiningFailed(const QString &errorMessage)
{
    startMiningButton->setEnabled(true);
    stopMiningButton->setEnabled(false);
    hashRateLabel->setText(tr("Mining failed: %1").arg(errorMessage));
}

QString MiningView::formatHashRate(double hashRate) const
{
    QString unit;
    if (hashRate > 1e9) {
        hashRate /= 1e9;
        unit = "GH/s";
    } else if (hashRate > 1e6) {
        hashRate /= 1e6;
        unit = "MH/s";
    } else if (hashRate > 1e3) {
        hashRate /= 1e3;
        unit = "KH/s";
    } else {
        unit = "H/s";
    }
    return QString("%1 %2").arg(hashRate, 0, 'f', 2).arg(unit);
}

void MiningView::onHashRateChanged(double hashRate)
{
    hashRateLabel->setText(tr("Hash Rate: %1").arg(formatHashRate(hashRate)));
}

void MiningView::updateNetworkHashRate()
{
    JSONRPCRequest request;
    request.strMethod = "getnetworkhashps";
    request.params = UniValue(UniValue::VARR);
    request.params.push_back(1);  // Get the network hash rate for the last block

    try {
        UniValue result = tableRPC.execute(request);
        double networkHashRate = result.get_real();
        networkHashRateLabel->setText(tr("Network Hash Rate: %1").arg(formatHashRate(networkHashRate)));
    } catch (const UniValue& objError) {
        networkHashRateLabel->setText(tr("Network Hash Rate: unavailable"));
    }
}