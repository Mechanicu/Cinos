package main

import (
	"context"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	pb "test/sdcs/sdcs_grpc"
	"time"
)

// http service listen address
const http_ipaddr string = "0.0.0.0"
const http_start_port uint16 = 9527
const http_pattern string = "/"
const invalid_nodeid int32 = -1

var local_cache map[SdcsCacheKey]SdcsCacheValue
var local_nodeid int32

// http get handler
func http_get_handler(key *SdcsCacheKey, val *SdcsCacheValue) bool {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	req := &pb.GetDataReq{Key: string(*key)}
	// try get from local cache
	rsp, err := grpc_client[local_nodeid].GetData(ctx, req)
	if err == nil && rsp.Res && rsp.NodeId != invalid_nodeid {
		rsp, err = grpc_client[rsp.NodeId].GetData(ctx, req)
	}
	if err != nil || !rsp.Res {
		if err != nil {
			log.Printf("Grpc get data failed, local:%d, err:%v", local_nodeid, err)
		}
		return false
	}
	val.val = rsp.Val

	// log.Printf("Http get, local:%d, remote:%d, key:%s, val:%s", local_nodeid, rsp.NodeId, *key, val.val)
	return true
}

// http post handler
func http_post_handler(key *SdcsCacheKey, val *SdcsCacheValue) bool {
	var err error
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	for i := 0; i < node_count; i++ {
		if i == int(local_nodeid) {
			_, err = grpc_client[i].SetData(ctx, &pb.SetDataReq{Key: string(*key), Val: val.val})
		} else {
			_, err = grpc_client[i].SetIndex(ctx, &pb.SetIndexReq{Key: string(*key), NodeId: local_nodeid})
		}
		if err != nil {
			log.Printf("Grpc sync data failed, local:%d, remote:%d, err:%v", local_nodeid, i, err)
			return false
		}
	}
	return true
}

// http delete handler
func http_delete_func(key *SdcsCacheKey) bool {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	for i := 0; i < node_count; i++ {
		rsp, err := grpc_client[i].DeleteIndex(ctx, &pb.DeleteIndexReq{Key: string(*key)})
		if err != nil {
			log.Printf("Grpc sync data failed, local:%d, remote:%d, err:%v", local_nodeid, i, err)
			return false
		}
		if !rsp.Res {
			return false
		}
	}
	return true
}

func http_body_parse(body string, key *SdcsCacheKey, val *SdcsCacheValue) bool {
	substrings := strings.Split(body, "\"")
	if len(substrings) < 4 {
		return false
	}
	val.val = substrings[3]
	val.nodeid = invalid_nodeid
	*key = SdcsCacheKey(substrings[1])
	return true
}

// http request handler
func http_handler(w http.ResponseWriter, r *http.Request) {
	// if method is post, key not included in url
	if r.Method == http.MethodPost {
		body, err := io.ReadAll(r.Body)
		if err != nil {
			http.Error(w, "Unable to read request body.", http.StatusInternalServerError)
			return
		}
		var key SdcsCacheKey
		var val SdcsCacheValue
		res := http_body_parse(string(body), &key, &val)
		// log.Printf("Http post, node:%d, body:%s, key:%s, val:%s", local_nodeid, body, key, val.val)
		if res {
			res = http_post_handler(&key, &val)
		}
		if res {
			w.WriteHeader(http.StatusOK)
		} else {
			w.WriteHeader(http.StatusNotFound)
		}
		return
	}

	// if not post, then key is included in url
	sub_strings := strings.Split(r.URL.String(), "/")
	key := sub_strings[len(sub_strings)-1]
	var val SdcsCacheValue
	switch r.Method {
	case http.MethodGet:
		res := http_get_handler((*SdcsCacheKey)(&key), &val)
		if res {
			w.WriteHeader(http.StatusOK)
			w.Header().Set("Content-Type", "application/json")
			fmt.Fprintln(w, "{\""+key+"\":\""+val.val+"\"}")
		} else {
			w.WriteHeader(http.StatusNotFound)
		}
	case http.MethodDelete:
		var count uint32 = 0
		res := http_delete_func((*SdcsCacheKey)(&key))
		if res {
			count++
		}
		w.WriteHeader(http.StatusOK)
		fmt.Fprintln(w, strconv.Itoa(int(count)))
	default:
		http.Error(w, "Unsupport http request.", http.StatusMethodNotAllowed)
	}
}

// http server thread
func http_server_start(
	_addr GrpcAddress,
	_handler func(w http.ResponseWriter, r *http.Request),
	_pattern string) {

	// register handler
	http.HandleFunc(_pattern, _handler)
	// start handler on current thread
	log.Printf("Start http server, node:%d, ip:%s, port:%d", local_nodeid, _addr.ipaddr, _addr.port)
	local_cache = make(map[SdcsCacheKey]SdcsCacheValue)
	err := http.ListenAndServe(
		_addr.ipaddr+":"+strconv.Itoa(int(_addr.port)),
		nil)
	if err != nil {
		log.Fatalf("Start http server failed:%s", err)
	}
	os.Exit(1)
}

const retry_sleep_time time.Duration = 1 << 10 << 10 << 10

func main() {
	// get current node id from cmd args
	if len(os.Args) < 2 {
		log.Fatalf("input:{current node id}")
		os.Exit(1)
	}
	current_node_id, err := strconv.Atoi(os.Args[1])
	if err != nil || current_node_id >= len(grpc_address) {
		log.Fatalf("Get node id failed")
		os.Exit(1)
	}
	var http_address = GrpcAddress{http_ipaddr, http_start_port + uint16(current_node_id)}
	local_nodeid = int32(current_node_id)

	// create http server
	go http_server_start(http_address, http_handler, http_pattern)
	// start grpc server
	grpc_server := SdcsGrpcServerInit(local_nodeid, &local_cache)
	go SdcsGrpcServerStart(grpc_address[local_nodeid].ipaddr, grpc_address[local_nodeid].port, grpc_server)
	for i := 0; i < node_count; {
		new_client := SdcsGrpcClientInit(grpc_address[i].ipaddr, grpc_address[i].port)
		if new_client == nil {
			log.Printf("Connect to grpc server failed, ip:%s, port:%d", grpc_address[i].ipaddr, grpc_address[i].port)
			time.Sleep(retry_sleep_time)
			continue
		}
		grpc_client[i] = *new_client
		log.Printf("Connect to grpc server success, local:%d, node:%d, ip:%s, port:%d",
			local_nodeid, i, grpc_address[i].ipaddr, grpc_address[i].port)
		i++
	}
	select {}
}
