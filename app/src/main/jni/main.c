#include "com_example_maye_jnitest_MainActivity.h"

JNIEXPORT jstring JNICALL Java_com_example_maye_jnitest_MainActivity_StringFromJNI(JNIEnv *env, jobject this)
{
    return (*env)->NewStringUTF(env, "Hello World From JNI!");
}