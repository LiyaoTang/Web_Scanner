// Java client program for ANU's comp3310 sockets Lab
// Peter Strazdins, RSCS ANU, 03/18

import java.io.*;
import java.net.*;

public class ClientTCP {
    static int port = 3310;
    
    public static void main (String[] args) throws IOException {
	if (args.length >= 1)
	    port = Integer.parseInt(args[0]);
        Socket sock = new Socket("localhost", port);
        System.out.println("client: created socket connected to local port " +
			   sock.getLocalPort() + " and to remote address " +
			   sock.getInetAddress() + " and port " +
			   sock.getPort());

        DataOutputStream out = new DataOutputStream(sock.getOutputStream());
        DataInputStream in = new DataInputStream(sock.getInputStream());
	
	BufferedReader stdIn
	    = new BufferedReader(new InputStreamReader(System.in));
	String outMsg;
	
	while ((outMsg = stdIn.readLine()) != null) {
	    //System.out.println("client: sending message: " + outMsg);
	    out.writeUTF(outMsg);
	    String inMsg;
	    try {
		inMsg = in.readUTF();
	    } catch (EOFException eof) {
		break;
	    }
	    System.out.println(inMsg);
	}
        sock.close();
	System.out.println("client: closed socket and terminating");
    }//main()
}//Client
