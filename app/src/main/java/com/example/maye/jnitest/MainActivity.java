package com.example.maye.jnitest;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //setContentView(R.layout.activity_main);
        TextView textView = new TextView(this);
        textView.setText(StringFromJNI());
        setContentView(textView);
    }

    public native String StringFromJNI();
    static {
        System.loadLibrary("jnitest");
    }
}
