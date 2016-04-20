package com.example.maye.IVI;

import android.content.Intent;
import android.net.VpnService;
import android.os.Handler;
import android.os.Looper;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.Date;
import java.util.Enumeration;
import java.util.Timer;

public class MainActivity extends AppCompatActivity {
    private Thread IVIthread; //native C thread
    private Thread BackGroundthread; //background thread
    private IpPacket ip_packet;
    private  GuiInfo guiInfo;
    private Date startTime;
    private int isStart = 0;
    private long sendBytes, receiveBytes, sendPackets, receivePackets;
    String ipHandleName = "/data/data/com.example.maye.IVI/myfifo";
    String statsHandleName = "/data/data/com.example.maye.IVI/myfifo_stats";
    private final String TAG = "MainActivity";
    //user interface components
    private Button startButton;
    private TextView time_info;
    private TextView speed_info;
    private TextView send_info;
    private TextView receive_info;
    private TextView ipv4_addr;
    private TextView ipv6_addr;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        sendBytes = 0;
        receiveBytes = 0;
        sendPackets = 0;
        receivePackets = 0;
        //mapping GUI to variables
        startButton = (Button) findViewById(R.id.start_service);
        time_info = (TextView) findViewById(R.id.time_duration);
        speed_info = (TextView) findViewById(R.id.speed_info);
        send_info = (TextView) findViewById(R.id.send_info);
        receive_info = (TextView) findViewById(R.id.receive_info);
        ipv4_addr = (TextView) findViewById(R.id.ipv4_addr);
        ipv6_addr = (TextView) findViewById(R.id.ipv6_addr);
        guiInfo = new GuiInfo();
        //create C thread
        IVIthread = new Thread(){
            @Override
            public void run(){
                IVI();
            }
        };


        //create Background thread
        BackGroundthread = new Thread(){
            @Override
            public void run(){
                //read for ip info
                ip_packet = readIpHandle();
                guiInfo.ipv4_addr = ip_packet.ipAddress;
                //create vpnService
                Intent intent = VpnService.prepare(MainActivity.this);
                if (intent != null) {
                    startActivityForResult(intent, 0);
                } else {
                    onActivityResult(0, RESULT_OK, null);
                }
                //start timer for GUI refreshing
                startTime = new Date();
                Timer timer = new Timer();
                timer.schedule(new MyTask(), 500, 1000);//500ms delay and 1s cycle
            }
        };

        //push button to start service
        startButton.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                guiInfo.ipv6_addr = getLocalIpAddress();
                if (isStart == 0 && guiInfo.ipv6_addr != null) {
                    IVIthread.start();
                    BackGroundthread.start();
                    isStart = 1;
                }else if (guiInfo.ipv6_addr == null){
                    Log.d(TAG, "network unavailable!");
                }
            }
        });
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
            intent.putExtra("socket", ip_packet.socket);
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
                temp_ip_packet.socket = piece[5];
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
        String socket;
    }

    class GuiInfo{
        String time_duration;
        String speed_info;
        String send_info;
        String receive_info;
        String ipv4_addr;
        String ipv6_addr;
    }

    //get Local Network information
    public String getLocalIpAddress() {
        try {
            for (Enumeration<NetworkInterface> en = NetworkInterface
                    .getNetworkInterfaces(); en.hasMoreElements();) {
                NetworkInterface intf = en.nextElement();
                for (Enumeration<InetAddress> enumIpAddr = intf
                        .getInetAddresses(); enumIpAddr.hasMoreElements();) {
                    InetAddress inetAddress = enumIpAddr.nextElement();
                    if (!inetAddress.isLoopbackAddress()) {
                        return inetAddress.getHostAddress().toString();
                    }
                }
            }
        } catch (SocketException ex) {
            ex.printStackTrace();
        }
        return null;
    }

    //timer routine
    class MyTask extends java.util.TimerTask{
        public void run(){
            File file = new File(statsHandleName);
            while (!file.exists()){
                try {
                    Thread.sleep(100);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            }
            try {
                FileInputStream fileInputStream = new FileInputStream(file);
                BufferedInputStream in = new BufferedInputStream(fileInputStream);
                byte buf[] = new byte[1024];
                int readLen = in.read(buf, 0, 1024);
                if (readLen > 0){
                    String ipStatInfo = new String(buf);
                    String piece[] = ipStatInfo.split(" ");
                    guiInfo.speed_info = "Speed: Upload "+piece[0]+" Bytes/s  Download "+piece[2]+" Bytes/s";
                    sendBytes += Long.parseLong(piece[0]);
                    sendPackets += Long.parseLong(piece[1]);
                    receiveBytes += Long.parseLong(piece[2]);
                    receivePackets += Long.parseLong(piece[3]);
                    guiInfo.send_info = "Send: " + Long.toString(sendBytes) + " Bytes / " + Long.toString(sendPackets) + " packets";
                    guiInfo.receive_info = "Receive: " + Long.toString(receiveBytes) + " Bytes / " + Long.toString(receivePackets) + " packets";
                    long time_dura = (new Date().getTime()-startTime.getTime())/1000;
                    long hour = time_dura/3600;
                    long min = (time_dura % 3600)/60;
                    long sec = time_dura % 60;
                    guiInfo.time_duration = "Time Duration: " + hour + " h " + min + " m " + sec + " s";
                    refreshGUI();
                }
                in.close();
            }catch (Exception e){
                e.printStackTrace();
            }
        }
    }

    protected void refreshGUI(){
        Log.v(TAG, "refreshing");
        new Handler(Looper.getMainLooper()).post(new Runnable() {
            @Override
            public void run() {
                time_info.setText(guiInfo.time_duration);
                speed_info.setText(guiInfo.speed_info);
                send_info.setText(guiInfo.send_info);
                receive_info.setText(guiInfo.receive_info);
                ipv4_addr.setText(guiInfo.ipv4_addr);
                ipv6_addr.setText(guiInfo.ipv6_addr);
            }
        });
    }

    //native JNI declaration
    public native String StringFromJNI();
    public native void IVI();
    static {
        System.loadLibrary("IVI");
    }
}
