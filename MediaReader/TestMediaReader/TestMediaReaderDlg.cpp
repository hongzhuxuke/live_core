
// TestMediaReaderDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "TestMediaReader.h"
#include "TestMediaReaderDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "AvPlayer.h"

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


// CTestMediaReaderDlg 对话框



CTestMediaReaderDlg::CTestMediaReaderDlg(CWnd* pParent /*=NULL*/)
: CDialogEx(CTestMediaReaderDlg::IDD, pParent) {
   m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTestMediaReaderDlg::DoDataExchange(CDataExchange* pDX) {
   CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTestMediaReaderDlg, CDialogEx)
   ON_WM_SYSCOMMAND()
   ON_WM_PAINT()
   ON_WM_QUERYDRAGICON()
   ON_BN_CLICKED(IDOK, &CTestMediaReaderDlg::OnBnClickedOk)
   ON_BN_CLICKED(IDC_VIDEO_NEXT100S, &CTestMediaReaderDlg::OnBnClickedVideoNext100s)
   ON_BN_CLICKED(IDC_NEXT_MEDIA, &CTestMediaReaderDlg::OnBnClickedNextMedia)
END_MESSAGE_MAP()


// CTestMediaReaderDlg 消息处理程序

BOOL CTestMediaReaderDlg::OnInitDialog() {
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

   mMediaReader = CreateMediaReader();
   mStart = false;
   return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CTestMediaReaderDlg::OnSysCommand(UINT nID, LPARAM lParam) {
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

void CTestMediaReaderDlg::OnPaint() {
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
HCURSOR CTestMediaReaderDlg::OnQueryDragIcon() {
   return static_cast<HCURSOR>(m_hIcon);
}

void CTestMediaReaderDlg::GetDataLoop() {
   FILE* pcmFile = NULL;
   fopen_s(&pcmFile, "d:\\mediaoutput.pcm", "wb");
   FILE* yuvFile = NULL;
   fopen_s(&yuvFile, "d:\\mediaoutput.yuv", "wb");
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
   mMediaOutput->GetAudioParam(channels, samplesPerSec, bitsPerSample);
   mMediaOutput->GetVideoParam(videoWidth, videoHeight, frameDur, framTimescale);

   while (mStart == true) {
      if (audioTime <= videoTime) {
         ret = mMediaOutput->GetNextAudioBuffer(&buffer, &numFrames, &audioTime);
         if (ret) {
            mAvPlayer->OnNewAudioSample(buffer, numFrames* bitsPerSample / 8 * channels);
            fwrite(buffer, 1, numFrames* bitsPerSample / 8 * channels, pcmFile);
            fflush(pcmFile);
         }
      } else {
         unsigned long long bufferSize;
         ret = mMediaOutput->GetNextVideoBuffer(&buffer, &bufferSize, &videoTime);
         if (ret) {
            if (mAvPlayer) {
               mAvPlayer->OnNewVideoFrame((LPVOID)buffer, bufferSize);
            }
            fwrite(buffer, 1, bufferSize, yuvFile);
            fflush(yuvFile);
         }
      }
   }
   fclose(pcmFile);
   fclose(yuvFile);
}

void CTestMediaReaderDlg::OnBnClickedOk() {
   // TODO:  在此添加控件通知处理程序代码
   mStart = !mStart;
   if (mStart == true) {
      mAvPlayer = new CAVPlayer();


      mMediaReader->SetPlaylistLoop(true);
      mMediaReader->SetVolume(100);

      void* newMedia = mMediaReader->AddNewMedia("D:\\1.flv");
      mMediaList.push_back(newMedia);
      newMedia = mMediaReader->AddNewMedia("D:\\1.flv");
      mMediaList.push_back(newMedia);

      mMediaOutput = mMediaReader->GetMediaOut();
      mCaptureThread = CreateThread(NULL, 0, CTestMediaReaderDlg::GetDataThread, this, 0, NULL);
      mMediaOutput->Start();

      mAvPlayer->SetVideoWindow(GetDlgItem(IDC_VIDEO_WND)->GetSafeHwnd(), 320, 240);
      mAvPlayer->OpenVideoRender();

      mAvPlayer->SetAudioOpt(16, 2, 44100, 100);
      mAvPlayer->MuteAudio(false);
      mAvPlayer->OpenAudioPlayer(true);

   } else {

   }


   //IDC_VIDEO_WND
   //CDialogEx::OnOK();
}
DWORD __stdcall CTestMediaReaderDlg::GetDataThread(LPVOID lpUnused) {
   CTestMediaReaderDlg* thisObj = (CTestMediaReaderDlg*)lpUnused;
   CoInitialize(0);
   thisObj->GetDataLoop();
   CoUninitialize();
   return 0;

}


void CTestMediaReaderDlg::OnBnClickedVideoNext100s() {
   // TODO:  在此添加控件通知处理程序代码

   mMediaReader->Seek(NULL, 200000);
}


void CTestMediaReaderDlg::OnBnClickedNextMedia() {
   // TODO:  在此添加控件通知处理程序代码
   static int mediaIndex = 0;
   mediaIndex ++ ;
   if (mediaIndex >= mMediaList.size()  )
      mediaIndex = 0;
   mMediaReader->PlayMedia(mMediaList[mediaIndex]);
}
