package com.example.maye.jnitest;

import android.content.Intent;
import android.net.VpnService;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;

public class MainActivity extends AppCompatActivity {
    private Thread IVIthread; //native C thread
    private Thread BackGroundthread;
    private IpPacket ip_packet;
    //int flag = 0;
    String ipHandleName = "/tmp/myfifo";
    String statsHandleName = "/tmp/myfifo_stats";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        //create and start C thread
        IVIthread = new Thread(){
            @Override
            public void run(){
                IVI();
            }
        };
        IVIthread.start();

        //create and start background thread
        BackGroundthread = new Thread(){
            @Override
            public void run(){
                //read for ip info
                ip_packet = readIpHandle();

                //create vpnService
                Intent intent = VpnService.prepare(MainActivity.this);
                if (intent != null) {
                    startActivityForResult(intent, 0);
                } else {
                    onActivityResult(0, RESULT_OK, null);
                }
            }
        };
        BackGroundthread.start();
        //Demo text setting
        TextView textView = new TextView(this);
        textView.setText(StringFromJNI());
        setContentView(textView);
    }


    protected void onActivityResult(int request, int result, Intent data) {
        if (result == RESULT_OK) {
            Intent intent = new Intent(this, MyVpnService.class);
            //pass information which vpn service need
            intent.putExtra("ip", ip_packet.ipAddress);
            intent.putExtra("route", ip_packet.route);
            intent.putExtra("DNS1", ip_packet.DNS1);
            intent.putExtra("DNS2", ip_packet.DNS2);
            intent.putExtra("DNS3", ip_packet.DNS3);
            startService(intent);
        }
    }

    protected IpPacket readIpHandle(){
        File file = new File(ipHandleName);
        while (!file.exists()){
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        IpPacket temp_ip_packet = null;
        try {
            FileInputStream fileInputStream = new FileInputStream(file);
            BufferedInputStream in = new BufferedInputStream(fileInputStream);
            byte buf[] = new byte[1024];
            int readLen = in.read(buf, 0, 1024);
            if (readLen > 0){
                String ipDataInfo = new String(buf);
                String piece[] = ipDataInfo.split(" ");
                temp_ip_packet = new IpPacket();
                temp_ip_packet.ipAddress = piece[0];
                temp_ip_packet.route = piece[1];
                temp_ip_packet.DNS1 = piece[2];
                temp_ip_packet.DNS2 = piece[3];
                temp_ip_packet.DNS3 = piece[4];
            }
            in.close();
        }catch (Exception e){
            System.out.println(e);
        }
        return temp_ip_packet;
    }

    class IpPacket{
        String ipAddress;
        String route;
        String DNS1,DNS2,DNS3;
    }

    //native JNI declaration
    public native String StringFromJNI();
    public native void IVI();
    static {
        System.loadLibrary("jnitest");
    }
}
