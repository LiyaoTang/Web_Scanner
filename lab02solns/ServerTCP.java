// Java server program for ANU's comp3310 sockets Lab 
// Peter Strazdins, RSCS ANU, 03/18

import java.io.*;
import java.net.*;

public class ServerTCP {
    static int port = 3310;
    public static void main (String[] args) throws IOException {
	if (args.length >= 1)
	    port = Integer.parseInt(args[0]);
        ServerSocket sSock = new ServerSocket(port);
        System.out.println("server: created socket with port number " +
			   sSock.getLocalPort()); 

        Socket sock = sSock.accept();
        System.out.println("server: received connection from "   +
                           sock.getRemoteSocketAddress() + ", " +
                           "now on port " + sock.getLocalPort());

        DataInputStream in = new DataInputStream(sock.getInputStream());
        DataOutputStream out = new DataOutputStream(sock.getOutputStream());
        String buf;

	while (true) {
	    try {
		buf = in.readUTF();
	    } catch (EOFException oef) {
		break;
	    }
	    System.out.println("server: received message: " + buf);
	    out.writeUTF(buf);
	    System.out.println("server: sent message " + buf);
	}
        sock.close();
        sSock.close();
	System.out.println("server: closed sockets and terminating");
    }//main()
}//Server
