package main

import (
	"encoding/binary"
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/jacobsa/go-serial/serial"
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
	bo.PutUint32(buf[c:], d.LoopTime)
	c += 4

	for i := range d.Nodes {
		bo.PutUint16(buf[c:], uint16(len(d.Nodes[i])))
		c += 2
		for _, v := range d.Nodes[i] {
			bo.PutUint32(buf[c:], v.Color)
			c += 4
			bo.PutUint32(buf[c:], uint32(float32(d.LoopTime)*v.LoopOff))
			c += 4
			bo.PutUint16(buf[c:], uint16(float32(numlights)*v.GradOff))
			c += 2
		}
	}

	if c != bytelen {
		log.Fatalf("Did not correctly serialize\n")
	}

	return buf
}

func parseDispJSON(body io.ReadCloser) *DispInfo {
	//parse JSON into a DispInfo
	dec := json.NewDecoder(body)

	var decobj DispInfo

	err := dec.Decode(&decobj)
	if err != nil {
		log.Printf("Unable to decode json\n")
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

func hardSerial() error {
	options := serial.OpenOptions{
		PortName:        "COM4",
		BaudRate:        115200,
		DataBits:        8,
		StopBits:        1,
		MinimumReadSize: 4,
	}
	log.Printf("Starting Serial Connection to %q\n", options.PortName)

	// TODO actually connect to the serial thing
	port, err := serial.Open(options)
	if err != nil {
		log.Fatalf("Serial.Open: %v", err)
	}

	defer port.Close()

	// start routine for debug communication
	go func() {
		debugbuf := make([]byte, 1024)
		i, err := port.Read(debugbuf)
		if err != nil {
			log.Fatal(err)
		}
		log.Printf("Device : %q\n", string(debugbuf[:i]))
	}()

	//TODO do routine that serializes and sends data down the line

	return nil
}

func debugSerial() error {
	const debugFPath string = "./Hardware/DebugLib/serialized.bin"
	const debugCPath string = "./Hardware/DebugLib/debugcmd"
	// start routine
	go func() {
		// read from the chan
		var d *DispInfo
		for {
			d = <-di
			log.Printf("Got Display Info:\n%#v\n\n", d)

			buf := d.Serialize(binary.LittleEndian, 150)

			// write debug file
			f, err := os.Create(debugFPath)
			if err != nil {
				log.Fatal(err)
			}
			_, err = f.Write(buf)
			if err != nil {
				log.Fatal(err)
			}

			log.Printf("Starting Library debugger:\n")

			// send buf to debug program
			// print the output from the debug program
			out, err := exec.Command(debugCPath, debugFPath).Output()
			if err != nil {
				log.Fatal(err)
			}
			log.Printf("\nDebugger :\n%q\n\n", string(out))
		}
	}()
	return nil
}

func callHard() error {
	// paramaterize this for different hardware or debugging
	return debugSerial()
	//return hardSerial()
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
	log.Printf("Starting Server on port %q\n", PORT)
	log.Fatal(http.ListenAndServe(PORT, nil))
}
