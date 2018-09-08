package main

import (
	"encoding/binary"
	"encoding/json"
	"io"
	"log"
	"net"
	"net/http"
	"path/filepath"
)

// Point Single color point in gradient
type Point struct {
	Color   uint32  `json:"c"`
	GradOff float32 `json:"g"`
	LoopOff float32 `json:"l"`
}

// DispInfo The nodes and information to disply the loop
type DispInfo struct {
	Nodes    [][]Point `json:"n"`
	LoopTime uint32    `json:"t"`
}

const hardwarePort string = ":8383"

// global for communication
var di chan *DispInfo

// Serialize used to make a serial array from a di
func (d DispInfo) Serialize(bo binary.ByteOrder, numlights uint16) []byte {
	// translate to light index and timeoffset
	// uin16_t bytes_in_message
	// int32_t looptime
	//  (for each node)
	//    uint16_t number of points
	//      (for each point)
	//        uint32_t color
	//        int32_t msoff
	//        uint16_t index
	bytelen := 6 // message_len in bytes + looptime in ms
	for i := range d.Nodes {
		bytelen += 2                             // len of points array
		bytelen += len(d.Nodes[i]) * (4 + 4 + 2) // count of points * sizeof(point)
	}

	buf := make([]byte, bytelen)
	c := 0

	bo.PutUint16(buf[c:], uint16(bytelen))
	c += 2
	bo.PutUint32(buf[c:], (d.LoopTime * 1000))
	c += 4

	for i := range d.Nodes {
		bo.PutUint16(buf[c:], uint16(len(d.Nodes[i]))) // len of node
		c += 2
		for _, v := range d.Nodes[i] {
			bo.PutUint32(buf[c:], v.Color) // color
			c += 4
			bo.PutUint32(buf[c:], uint32(float32(d.LoopTime*1000)*v.LoopOff)) // time offset
			c += 4
			bo.PutUint16(buf[c:], uint16(float32(numlights)*v.GradOff)) // index
			c += 2
		}
	}

	if c != bytelen {
		log.Fatalf("Did not correctly serialize")
	}

	return buf
}

func parseDispJSON(body io.ReadCloser) *DispInfo {
	//parse JSON into a DispInfo
	dec := json.NewDecoder(body)

	var decobj DispInfo

	err := dec.Decode(&decobj)
	if err != nil {
		log.Printf("Unable to decode json")
		return nil
	}

	return &decobj
}

func gradEditor(w http.ResponseWriter, req *http.Request) {
	// if it is a GET request, just serve the files
	switch req.Method {
	case "GET":
		// serve
		// sanitize fpath
		http.ServeFile(w, req, filepath.Join("./site", req.URL.Path))
	case "POST":
		log.Printf("Got a POST")
		// parse and tell
		di <- parseDispJSON(req.Body)
		// respond good
		http.Error(w, "Submitted", 204)
	default:
		// error
		http.Error(w, "Unsupported Method", 501)
	}
}

func callHard() error {
	log.Printf("Starting Socket Listener on port %v", hardwarePort)

	s, err := net.Listen("tcp", hardwarePort)
	if err != nil {
		return err
	}

	go func() {
		for {
			log.Println("\nWaiting for Accept...")
			con, err := s.Accept()
			if err != nil {
				log.Fatalf("Bad Accept: %v", err)
			}
			log.Printf("Got connection from %v", con.RemoteAddr())

			// if we get an updated display info, we will accept connections
			var buf []byte

			var newdisp *DispInfo
			found := false
			for found == false {
				select {
				case newdisp = <-di:
					found = true
				default:
					found = false
				}
			}

			if newdisp != nil {
				buf = newdisp.Serialize(binary.LittleEndian, 150)
				log.Printf("Sending out new pattern, should have size %x", binary.LittleEndian.Uint16(buf[:2]))
			} else {
				buf = []byte{0x0, 0x0} // nothing new
				log.Printf("No new pattern to send")
			}
			// if we wanted to use this in a robust way, we would use tls and check a client cert here
			// but if you want to mitm my leds, more power to you maybe I will put a ctf flag in there even.

			con.Write(buf[:2])
			if len(buf[2:]) > 0 {
				con.Write(buf[2:])
			}
			con.Close()
		}
	}()

	return nil
}

func main() {
	// make the chan
	di = make(chan *DispInfo)

	// talk to hardware
	err := callHard()
	if err != nil {
		log.Fatal(err)
	}

	// be a server
	var PORT = ":5077" // 0x13D5
	http.HandleFunc("/", gradEditor)
	log.Printf("Starting Server on port %q", PORT)
	log.Fatal(http.ListenAndServe(PORT, nil))
}
