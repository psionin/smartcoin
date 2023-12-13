from bitcoinrpc.authproxy import AuthServiceProxy, JSONRPCException
rpc_connection = AuthServiceProxy("http://%s:%s@127.0.0.1:8583"%("smartcoin", "smartcoin"))

num_blocks = rpc_connection.getblockcount()
print(num_blocks)

