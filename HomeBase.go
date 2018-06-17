package main

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
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
		log.Printf("Got a POST!")
		// parse and tell
		di <- parseDispJSON(req.Body)
		// respond good
		http.Error(w, "Submitted", 204)
	default:
		// error
		http.Error(w, "Unsupported Method", 501)
	}
}

func talkSerial() error {
	//var numlights uint16 = 150
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

	//start goroutines for serial communication
	go func() {
		debugbuf := make([]byte, 1024)
		i, err := port.Read(debugbuf)
		if err != nil {
			log.Fatal(err)
		}
		log.Printf("Arduino : %q\n", string(debugbuf[:i]))
	}()
	go func() {
		// read from the chan
		var d *DispInfo
		for {
			d = <-di
			log.Printf("Got Display Info:\n%#v\n\n", d)

			// translate to light index and timeoffset

			// send it along the serial chain

			// First # to send it the # of bytes for the whole thing
			// Then send each node as:
			//		Send # of points
			//		Send Each Point
			// TODO
		}
	}()

	return nil
}

func talkHardware() error {
	return talkSerial()
}

func main() {
	// make the chan
	di = make(chan *DispInfo)

	// talk to hardware
	err := talkHardware()
	if err != nil {
		log.Fatal(err)
	}

	// be a server
	var PORT = ":5077" // 0x13D5
	http.HandleFunc("/", gradEditor)
	log.Printf("Starting Server on port %q\n", PORT)
	log.Fatal(http.ListenAndServe(PORT, nil))
}
