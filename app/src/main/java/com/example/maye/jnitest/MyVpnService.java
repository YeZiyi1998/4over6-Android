package com.example.maye.jnitest;

import android.content.Intent;
import android.net.VpnService;
import android.os.ParcelFileDescriptor;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;

/**
 * Created by Silver_Bullet on 16/4/17.
 */
public class MyVpnService extends VpnService {
    private ParcelFileDescriptor mInterface;
    String ipHandleName = "/data/data/com.example.maye.jnitest/myfifo";
    //Configure a builder for the interface.
    Builder builder = new Builder();
    // Services interface
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        //a. Configure the TUN and get the interface.
        String ipAddress = intent.getStringExtra("ip");
        String route = intent.getStringExtra("route");
        String dns = intent.getStringExtra("DNS1");
        String socket = intent.getStringExtra("socket");
        protect(Integer.parseInt(socket));
        mInterface = builder.setSession("MyVPNService")
                .addAddress(ipAddress, 24)
                .addDnsServer(dns)
                .addRoute(route, 0).establish();
        try{
            File file = new File(ipHandleName);
            FileOutputStream fileOutputStream = new FileOutputStream(file);
            BufferedOutputStream out = new BufferedOutputStream(fileOutputStream);
            //transfer int to byte[]
            int fd = mInterface.getFd();
            ByteArrayOutputStream boutput = new ByteArrayOutputStream();
            DataOutputStream doutput = new DataOutputStream(boutput);
            doutput.writeInt(fd);
            byte[] buf = boutput.toByteArray();
            out.write(buf, 0, buf.length);
            out.flush();
            out.close();
        }catch(Exception e){
            e.printStackTrace();
        }
        //start the service
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        // TODO Auto-generated method stub
        super.onDestroy();
    }
}