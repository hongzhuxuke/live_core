
// TestDeckLinkDeviceDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "TestDeckLinkDevice.h"
#include "TestDeckLinkDeviceDlg.h"
#include "afxdialogex.h"

#include "IDeckLinkDevice.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx {
public:
   CAboutDlg();

   // 对话框数据
   enum { IDD = IDD_ABOUTBOX };

protected:
   virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

   // 实现
protected:
   DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD) {
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
   CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CTestDeckLinkDeviceDlg 对话框



CTestDeckLinkDeviceDlg::CTestDeckLinkDeviceDlg(CWnd* pParent /*=NULL*/)
: CDialogEx(CTestDeckLinkDeviceDlg::IDD, pParent) {
   m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
   mDeckLinkDevice = NULL;
}

void CTestDeckLinkDeviceDlg::DoDataExchange(CDataExchange* pDX) {
   CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTestDeckLinkDeviceDlg, CDialogEx)
   ON_WM_SYSCOMMAND()
   ON_WM_PAINT()
   ON_WM_QUERYDRAGICON()
   ON_BN_CLICKED(ID_CAPTURE_BTN, &CTestDeckLinkDeviceDlg::OnBnClickedCaptureBtn)
END_MESSAGE_MAP()


// CTestDeckLinkDeviceDlg 消息处理程序

BOOL CTestDeckLinkDeviceDlg::OnInitDialog() {
   CDialogEx::OnInitDialog();

   // 将“关于...”菜单项添加到系统菜单中。

   // IDM_ABOUTBOX 必须在系统命令范围内。
   ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
   ASSERT(IDM_ABOUTBOX < 0xF000);

   CMenu* pSysMenu = GetSystemMenu(FALSE);
   if (pSysMenu != NULL) {
      BOOL bNameValid;
      CString strAboutMenu;
      bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
      ASSERT(bNameValid);
      if (!strAboutMenu.IsEmpty()) {
         pSysMenu->AppendMenu(MF_SEPARATOR);
         pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
      }
   }

   // 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
   //  执行此操作
   SetIcon(m_hIcon, TRUE);			// 设置大图标
   SetIcon(m_hIcon, FALSE);		// 设置小图标

   // TODO:  在此添加额外的初始化代码


   InitDeckLinkDeviceManager();

   return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CTestDeckLinkDeviceDlg::OnSysCommand(UINT nID, LPARAM lParam) {
   if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
      CAboutDlg dlgAbout;
      dlgAbout.DoModal();
   } else {
      CDialogEx::OnSysCommand(nID, lParam);
   }
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CTestDeckLinkDeviceDlg::OnPaint() {
   if (IsIconic()) {
      CPaintDC dc(this); // 用于绘制的设备上下文

      SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

      // 使图标在工作区矩形中居中
      int cxIcon = GetSystemMetrics(SM_CXICON);
      int cyIcon = GetSystemMetrics(SM_CYICON);
      CRect rect;
      GetClientRect(&rect);
      int x = (rect.Width() - cxIcon + 1) / 2;
      int y = (rect.Height() - cyIcon + 1) / 2;

      // 绘制图标
      dc.DrawIcon(x, y, m_hIcon);
   } else {
      CDialogEx::OnPaint();
   }
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CTestDeckLinkDeviceDlg::OnQueryDragIcon() {
   return static_cast<HCURSOR>(m_hIcon);
}



void CTestDeckLinkDeviceDlg::OnBnClickedCaptureBtn() {
   // TODO:  在此添加控件通知处理程序代码
   if (mDeckLinkDevice == NULL) {
      const wchar_t* deckLinkName = GetDeckLinkDeviceName(0);
      if (deckLinkName)
         mDeckLinkDevice = GetDeckLinkDevice(deckLinkName);
      if (mDeckLinkDevice) {
         mDeckLinkDevice->EnableAudio(true);
         mDeckLinkDevice->EnableVideo(true,NULL);
         mDeckLinkDevice->StartCapture();
         mStart = true;
         mCaptureThread = CreateThread(NULL, 0, CTestDeckLinkDeviceDlg::GetDataThread, this, 0, NULL);
         if (!mCaptureThread) {
            DWORD error = GetLastError();
            /* gLogger->logError("ProcessMonitor::Start: CreateThread failed, error = 0x%08x.",
            error);*/
            return;
         }


      }
   } else {
      mStart = false;
      mDeckLinkDevice->StopCapture();
      mDeckLinkDevice = NULL;
   }

}


void CTestDeckLinkDeviceDlg::GetDataLoop() {
   FILE* pcmFile = NULL;
   fopen_s(&pcmFile, "d:\\decklink.pcm", "wb");
   FILE* yuvFile = NULL;
   fopen_s(&yuvFile, "d:\\decklink.yuv", "wb");
   bool ret = false;
   unsigned long long audioTime = 0;
   unsigned long long videoTime = 0;
   void *buffer = NULL;
   unsigned int numFrames;   
   unsigned int channels;
   unsigned int samplesPerSec;
   unsigned int bitsPerSample;
   unsigned long videoWidth;
   unsigned long videoHeight;
    long long frameDur;
    long long framTimescale;
   mDeckLinkDevice->GetAudioParam(channels, samplesPerSec, bitsPerSample);
   mDeckLinkDevice->GetVideoParam(videoWidth, videoHeight, frameDur, framTimescale);

   while (mStart == true) {
      if (audioTime <= videoTime) {
         ret = mDeckLinkDevice->GetNextAudioBuffer(&buffer, &numFrames, &audioTime);
         if (ret) {
            fwrite(buffer, 1, numFrames* bitsPerSample / 8 * channels, pcmFile);
            fflush(pcmFile);
         }
      } else {
         unsigned long long bufferSize;
         ret = mDeckLinkDevice->GetNextVideoBuffer(&buffer, &bufferSize, &videoTime);
         if (ret) {
            fwrite(buffer, 1, bufferSize, yuvFile);
            fflush(yuvFile);
         }
      }
   }
   fclose(pcmFile);
   fclose(yuvFile);
}
DWORD __stdcall CTestDeckLinkDeviceDlg::GetDataThread(LPVOID lpUnused) {
   CTestDeckLinkDeviceDlg* thisObj = (CTestDeckLinkDeviceDlg*)lpUnused;
   CoInitialize(0);
   thisObj->GetDataLoop();
   CoUninitialize();
   return 0;

}