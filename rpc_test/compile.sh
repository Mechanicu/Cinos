gcc \
test_rpcservice.c rpc_server/rpc_sever.c rpc_client/rpc_client.c rpc_daemon/rpc_service.c mem_pool/mem_pool.c hashlist/hashlist.c \
-lpthread \
-o rpcservice_test
