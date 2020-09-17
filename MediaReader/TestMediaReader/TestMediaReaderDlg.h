
// TestMediaReaderDlg.h : 头文件
//

#pragma once

#include "IMediaReader.h"
#include <vector>

class CAVPlayer;
// CTestMediaReaderDlg 对话框
class CTestMediaReaderDlg : public CDialogEx
{
// 构造
public:
	CTestMediaReaderDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
	enum { IDD = IDD_TESTMEDIAREADER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

   

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
   afx_msg void OnBnClickedOk();
   private:
   IMediaReader* mMediaReader;
   IMediaOutput* mMediaOutput;

   void GetDataLoop();
   static DWORD __stdcall GetDataThread(LPVOID lpUnused);
private:
   HANDLE mCaptureThread;
   bool mStart;
   CAVPlayer*        mAvPlayer;
   std::vector<void*> mMediaList;
public:
   afx_msg void OnBnClickedVideoNext100s();
   afx_msg void OnBnClickedNextMedia();
};
