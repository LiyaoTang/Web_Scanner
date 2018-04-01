// Java client program for ANU's comp3310 sockets Lab
// Peter Strazdins, RSCS ANU, 03/18

import java.io.*;
import java.net.*;

public class ClientUDP {
    static int port = 3310;
    static final int BUFLEN = 128;
    
    public static void main (String[] args) throws IOException {
	if (args.length >= 1)
	    port = Integer.parseInt(args[0]);
        DatagramSocket sock = new DatagramSocket();
	InetAddress serverAddr = InetAddress.getByName("localhost");
	System.out.println("client: created socket, attempting sendto port "
			   + port + " on server " + serverAddr);

       	BufferedReader stdIn
	    = new BufferedReader(new InputStreamReader(System.in));
	String outMsg;
	
	while ((outMsg = stdIn.readLine()) != null) {
	    byte[] outData = new byte[BUFLEN];
	    outData = outMsg.getBytes();
	    DatagramPacket outPacket =
		new DatagramPacket(outData, outData.length, serverAddr, port);
	    sock.send(outPacket);

	    byte[] inData = new byte[BUFLEN];
	    DatagramPacket inPacket =
		new DatagramPacket(inData, inData.length);
	    sock.receive(inPacket);
	    String inMsg = new String(inPacket.getData());
	    System.out.println(inMsg);
	}
        sock.close();
	System.out.println("client: closed socket and terminating");
    }//main()
}//ClientUDP
