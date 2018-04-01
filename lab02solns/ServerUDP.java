// Java server program for ANU's comp3310 sockets Lab 
// Peter Strazdins, RSCS ANU, 03/18

import java.io.*;
import java.net.*;

public class ServerUDP {
    static final int BUFLEN = 64;    
    static int port = 3310;
    public static void main (String[] args) throws IOException {
	if (args.length >= 1)
	    port = Integer.parseInt(args[0]);
        DatagramSocket sock = new DatagramSocket(port);
        System.out.println("server: created socket with port number " +
			   sock.getLocalPort()); 

	while (true) {
	    byte[] inData = new byte[BUFLEN];
	    DatagramPacket inPacket =
		new DatagramPacket(inData, inData.length);
	    sock.receive(inPacket);
	    String inMsg = new String(inPacket.getData());
	    InetAddress inAddr = inPacket.getAddress();
	    int inPort = inPacket.getPort();
	    System.out.println("server: received message from " + inAddr +
			       " on port " + inPort + ": " + inMsg);
	    
	    DatagramPacket outPacket =
	       new DatagramPacket(inData, inData.length, inAddr, inPort);
	    sock.send(outPacket);
	}
        //sock.close();
	//System.out.println("server: closed socket and terminating");
    }//main()
}//UDPServer
