#ifndef BITCOIN_QT_MININGVIEW_H
#define BITCOIN_QT_MININGVIEW_H

#include <QObject>
#include <QWidget>
#include <QKeyEvent>
#include <QThread>
#include <QTimer>

class WalletModel;
class ClientModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QTableView;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

class MiningWorker : public QObject
{
    Q_OBJECT

private:
    bool m_mining;
    QTimer* m_miningTimer;
    static constexpr int MINING_INTERVAL = 5000; // 5 seconds
    uint64_t m_maxTriesPerInterval;  // Calibrated value
    
    void calibrateMaxTries();  // New method

public:
    ~MiningWorker()
    {
        if (m_miningTimer) {
            m_miningTimer->stop();
            delete m_miningTimer;
            m_miningTimer = nullptr;
        }
    }
    explicit MiningWorker(QObject *parent = nullptr) : 
        QObject(parent), 
        m_mining(false),
        m_miningTimer(new QTimer(this)),
        m_maxTriesPerInterval(1000000) // Start with default, will be calibrated
    {
        connect(m_miningTimer, &QTimer::timeout, this, &MiningWorker::mineBlock);
    }

Q_SIGNALS:
    void miningStarted();
    void miningStopped();
    void miningFailed(const QString &errorMessage);
    void hashRateChanged(double hashRate);

public Q_SLOTS:
    void startMining();
    void stopMining();
    void mineBlock();
};

class MiningView : public QWidget
{
    Q_OBJECT

public:
    explicit MiningView(const PlatformStyle *platformStyle, QWidget *parent = nullptr);
    ~MiningView();

    void setClientModel(ClientModel *clientModel);
    void setWalletModel(WalletModel *walletModel);

private:
    ClientModel *clientModel;
    WalletModel *walletModel;
    
    QPushButton *startMiningButton;
    QPushButton *stopMiningButton;
    QLabel *hashRateLabel;
    QLabel *networkHashRateLabel;
    MiningWorker *miningWorker;
    QThread *miningThread;
    
    QString formatHashRate(double hashRate) const;

Q_SIGNALS:
    void startMiningSignal();
    void stopMiningSignal();

private Q_SLOTS:
    void startMining();
    void stopMining();
    void updateNetworkHashRate();
    void onMiningStopped();
    void onMiningFailed(const QString &errorMessage);
    void onHashRateChanged(double hashRate);
};

#endif // BITCOIN_QT_MININGVIEW_H