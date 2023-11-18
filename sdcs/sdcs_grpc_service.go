package main

import (
	"context"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"log"
	"net"
	"strconv"
	pb "test/sdcs/sdcs_grpc"
)

type SdcsCacheKey string
type SdcsCacheValue struct {
	val    string
	nodeid int32
}
type GrpcAddress struct {
	ipaddr string
	port   uint16
}
type SdcsGrpcServer struct {
	pb.UnimplementedSdcsSyncServer
	cache  *map[SdcsCacheKey]SdcsCacheValue
	nodeid int32
}

// grpc service listen address
const grpc_ipaddr_test string = "127.0.0.1"
const grpc_container_name_prefix string = "server"
const grpc_ipaddr = grpc_container_name_prefix
const grpc_start_port uint16 = 50000
const node_count = 3

var grpc_address = []GrpcAddress{
	{grpc_ipaddr + "1", grpc_start_port},
	{grpc_ipaddr + "2", grpc_start_port + 1},
	{grpc_ipaddr + "3", grpc_start_port + 2},
}
var grpc_client [node_count]pb.SdcsSyncClient

// grpc service method implement
func (s *SdcsGrpcServer) SetIndex(ctx context.Context, req *pb.SetIndexReq) (*pb.SetIndexRep, error) {
	var err error = nil
	cache := s.cache
	(*cache)[SdcsCacheKey(req.Key)] = SdcsCacheValue{"", req.NodeId}
	// log.Printf("Set index, local:%d, remote:%d", local_nodeid, req.NodeId)
	return &pb.SetIndexRep{Res: true}, err
}

func (s *SdcsGrpcServer) SetData(ctx context.Context, req *pb.SetDataReq) (*pb.SetDataRep, error) {
	var err error = nil
	cache := s.cache
	val := SdcsCacheValue{val: req.Val, nodeid: invalid_nodeid}
	(*cache)[SdcsCacheKey(req.Key)] = val
	// log.Printf("Set data, local:%d, val:%s", local_nodeid, val.val)
	return &pb.SetDataRep{Res: true}, err
}

func (s *SdcsGrpcServer) GetData(ctx context.Context, req *pb.GetDataReq) (*pb.GetDataRep, error) {
	var err error = nil
	cache := s.cache
	val, res := (*cache)[SdcsCacheKey(req.Key)]
	// log.Printf("Get data, local:%d, val:%s, remote:%d", local_nodeid, val.val, val.nodeid)
	return &pb.GetDataRep{Val: val.val, NodeId: val.nodeid, Res: res}, err
}

func (s *SdcsGrpcServer) DeleteIndex(ctx context.Context, req *pb.DeleteIndexReq) (*pb.DeleteIndexRep, error) {
	var err error = nil
	cache := s.cache
	_, res := (*cache)[SdcsCacheKey(req.Key)]
	if res {
		delete(*cache, SdcsCacheKey(req.Key))
	}
	return &pb.DeleteIndexRep{Res: res}, err
}

func SdcsGrpcServerInit(lnode int32, lcache *map[SdcsCacheKey]SdcsCacheValue) *SdcsGrpcServer {
	s := new(SdcsGrpcServer)
	s.cache = lcache
	s.nodeid = lnode
	log.Printf("Init grpc server successfully, cache:%p, node:%d", s.cache, s.nodeid)
	return s
}

func SdcsGrpcClientInit(ip string, port uint16) *pb.SdcsSyncClient {
	var new_client *pb.SdcsSyncClient = nil
	conn, err := grpc.Dial(ip+":"+strconv.Itoa(int(port)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		log.Println("Create connector failed")
		return new_client
	}

	new_client = new(pb.SdcsSyncClient)
	*new_client = pb.NewSdcsSyncClient(conn)
	return new_client
}

func SdcsGrpcServerStart(ip string, port uint16, s *SdcsGrpcServer) {
	listen, err := net.Listen("tcp", ip+":"+strconv.Itoa(int(port)))
	if err != nil {
		log.Fatalf("Failed to create listener for grpc server: %v", err)
	}

	var opts []grpc.ServerOption
	grpc_server := grpc.NewServer(opts...)
	pb.RegisterSdcsSyncServer(grpc_server, s)
	log.Printf("Start grpc server, node:%d, ip:%s, port:%d", local_nodeid, ip, port)
	grpc_server.Serve(listen)
}
