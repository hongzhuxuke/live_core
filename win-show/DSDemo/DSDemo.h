#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DSDemo.h"
#include "..\IDshowEngine.h"
class DSDemo : public QMainWindow
{
    Q_OBJECT

public:
    DSDemo(QWidget *parent = Q_NULLPTR);
    static void RecvVideoCallBack(const DShow::VideoConfig &config, unsigned char *data,
       size_t size, long long startTime, long long stopTime,
       long rotation);
    static void HDCRender(int type, HWND desHwnd, LPBYTE bytes, int cx, int cy);
private slots:
   void OnDeviceSelect(int);
   void slot_Create();
   void slot_Destory() ;
   void slot_Start() ;
   void slot_Stop() ;
private:
    Ui::DSDemoClass ui;
    static HWND mHwnd;
};
