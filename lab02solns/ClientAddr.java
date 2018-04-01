// Java client program for ANU's comp3310 sockets Lab
// Peter Strazdins, RSCS ANU, 03/18

import java.io.*;
import java.net.*;

public class ClientAddr {
    static int port = 3310;
    
    public static void main (String[] args) throws IOException {
	if (args.length >= 1)
	    port = Integer.parseInt(args[0]);
        Socket sock = new Socket("fe80::da9e:f3ff:fe05:7f1b", port);
        System.out.println("client: created socket connected to local port " +
			   sock.getLocalPort() + " and to remote address " +
			   sock.getInetAddress() + " and port " +
			   sock.getPort());

        DataOutputStream out = new DataOutputStream(sock.getOutputStream());
	String outMsg = "Hello Server";
        out.writeUTF(outMsg);
	System.out.println("client: sending message: " + outMsg);

        DataInputStream in = new DataInputStream(sock.getInputStream());
	String inMsg = in.readUTF();
        System.out.println("client: received message: " + inMsg);

        sock.close();
	System.out.println("client: closed socket and terminating");
    }//main()
}//Client
