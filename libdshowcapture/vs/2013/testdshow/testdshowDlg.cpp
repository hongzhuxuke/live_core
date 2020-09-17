
// testdshowDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "Logging.h"
#include "testdshow.h"
#include "testdshowDlg.h"
#include "afxdialogex.h"

#include "dshowcapture.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
using namespace  DShow;
#define DEBUG_DS_AUDIO_CAPTURE
#ifdef DEBUG_DS_AUDIO_CAPTURE  
FILE*    mRawFile;
#endif

void audioReceive(const AudioConfig &config,
                  unsigned char *data, size_t size,
                  long long startTime, long long stopTime) {

#ifdef DEBUG_DS_AUDIO_CAPTURE
   fwrite(data, 1, size, mRawFile);
   fflush(mRawFile);
#endif

   long startPts = startTime / 10000;
   long stopPts = stopTime / 10000;
}


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


// CtestdshowDlg 对话框



CtestdshowDlg::CtestdshowDlg(CWnd* pParent /*=NULL*/)
: CDialogEx(CtestdshowDlg::IDD, pParent) {
   m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CtestdshowDlg::DoDataExchange(CDataExchange* pDX) {
   CDialogEx::DoDataExchange(pDX);
   DDX_Control(pDX, IDC_AUDIO_DEVICE_LIST, mAudioDeviceList);
}

BEGIN_MESSAGE_MAP(CtestdshowDlg, CDialogEx)
   ON_WM_SYSCOMMAND()
   ON_WM_PAINT()
   ON_WM_QUERYDRAGICON()
   ON_BN_CLICKED(ID_CAPTURE, &CtestdshowDlg::OnBnClickedCapture)
   ON_BN_CLICKED(IDCANCEL, &CtestdshowDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CtestdshowDlg 消息处理程序

BOOL CtestdshowDlg::OnInitDialog() {
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
   mAudioDevices.clear();
   DShow::Device::EnumAudioDevices(mAudioDevices);
   mAudioDeviceList.ResetContent();
   for (int i = 0; i < mAudioDevices.size(); i++) {
      mAudioDeviceList.AddString(mAudioDevices[i].name.c_str());
   }



   return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CtestdshowDlg::OnSysCommand(UINT nID, LPARAM lParam) {
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

void CtestdshowDlg::OnPaint() {
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
HCURSOR CtestdshowDlg::OnQueryDragIcon() {
   return static_cast<HCURSOR>(m_hIcon);
}



void CtestdshowDlg::OnBnClickedCapture() {
   // TODO:  在此添加控件通知处理程序代码
#ifdef DEBUG_DS_AUDIO_CAPTURE
   // mAudioEncoder = CreateAACEncoder(32, GetSampleRateHz(), NumAudioChannels());
   //  mAacFile = fopen("d:\\aac.aac", "wb");
   fopen_s(&mRawFile, "d:\\test_ds_AudioCapture.pcm", "wb");

#endif 

   mAudioDevice = new Device(InitGraph::True);
   AudioConfig audioConfig;
   AudioConfig auctualConfig;
   audioConfig.callback = audioReceive;
   audioConfig.useVideoDevice = false;
   audioConfig.sampleRate = 44100;
   audioConfig.channels = 2;
   audioConfig.context = this;

   int curSel = mAudioDeviceList.GetCurSel();
   if (curSel < 0 || curSel >= mAudioDevices.size())
      curSel = 0;
   if (mAudioDevices.size()>0) {
      audioConfig.name = mAudioDevices[curSel].name;
      audioConfig.path = mAudioDevices[curSel].path;
      audioConfig.format = AudioFormat::Wave16bit;
      mAudioDevice->SetAudioConfig(&audioConfig);
      mAudioDevice->ConnectFilters();
      mAudioDevice->GetAudioConfig(auctualConfig);
      mAudioDevice->Start();
      //mAudioDevice->GetAudioDeviceId()
      //mAudioDevice->OpenDialog(NULL, DShow::DialogType::ConfigAudio);
   }

}


void CtestdshowDlg::OnBnClickedCancel() {
   // TODO:  在此添加控件通知处理程序代码

   mAudioDevice->Stop();
   CDialogEx::OnCancel();
#ifdef DEBUG_DS_AUDIO_CAPTURE   
   fclose(mRawFile);
#endif  

}
