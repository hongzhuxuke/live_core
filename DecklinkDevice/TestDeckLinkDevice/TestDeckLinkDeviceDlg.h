
// TestDeckLinkDeviceDlg.h : 头文件
//

#pragma once

class IDeckLinkDevice;
// CTestDeckLinkDeviceDlg 对话框
class CTestDeckLinkDeviceDlg : public CDialogEx {
   // 构造
public:
   CTestDeckLinkDeviceDlg(CWnd* pParent = NULL);	// 标准构造函数

   // 对话框数据
   enum { IDD = IDD_TESTDECKLINKDEVICE_DIALOG };

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
   afx_msg void OnBnClickedCaptureBtn();
private:
   IDeckLinkDevice* mDeckLinkDevice;

   void GetDataLoop();
   static DWORD __stdcall GetDataThread(LPVOID lpUnused);
private:
   HANDLE mCaptureThread;
   bool mStart;
};
