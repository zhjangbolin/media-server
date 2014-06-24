
// rtspclientDlg.cpp : implementation file
//

#include "stdafx.h"
#include "rtspclient.h"
#include "rtspclientDlg.h"
#include "rtsp-client.h"

#include "rtp-avp-udp.h"
#include "h264-source.h"

#include "sys/sock.h"
#include "sys/process.h"
#include "sys/system.h"
#include "sys/path.h"

#include "mpeg-ts.h"
#include "hls-server.h"
#include "H264Reader.h"

#include "http-server.h"
#include "StdCFile.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CrtspclientDlg dialog




CrtspclientDlg::CrtspclientDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CrtspclientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CrtspclientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CrtspclientDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CrtspclientDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CrtspclientDlg::OnBnClickedCancel)
END_MESSAGE_MAP()


// CrtspclientDlg message handlers
int STDCALL OnThread(IN void* param)
{
//	while(1)
//		aio_socket_process(200);
	return 0;
}

static void OnTSWrite(void* param, const void* packet, int bytes)
{
	FILE *fp = (FILE*)param;
	fwrite(packet, 1, bytes, fp);
}

static void OnReadH264(void* ts, const void* data, int bytes)
{
	static int64_t pcr = 0;
	static int64_t pts = 0;

	if(0 == pcr)
	{
		time_t t;
		time(&t);
		pts = 412536249;//t * 90000;
	}
	else
	{
		pts += 90 * 40; // 90kHZ * 40ms
	}

	pcr = pts * 300;
	mpeg_ts_write(ts, 0x1b, pts, pts, data, bytes);
}

static int OnHTTP(void* param, void* session, const char* method, const char* path)
{
	if(0 == strcmp("/1.m3u8", path))
	{
		const char* m3u8 = 
			"#EXTM3U\n"
			"#EXT-X-VERSION:3\n"
			"#EXT-X-TARGETDURATION:15\n"
			"#EXT-X-MEDIA-SEQUENCE:1\n"
			"#EXTINF:15,\n"
			"1.ts\n"
			"#EXTINF:15,\n"
			"2.ts\n"
			"#EXTINF:15,\n"
			"3.ts\n"
			"#EXT-X-ENDLIST\n";

		void* bundle = http_bundle_alloc(strlen(m3u8)+1);
		void* ptr = http_bundle_lock(bundle);
		strcpy((char*)ptr, m3u8);
		http_bundle_unlock(bundle, strlen(m3u8)+1);

		http_server_set_content_type(session, "application/vnd.apple.mpegurl");
		http_server_send(session, 200,bundle);
		http_bundle_free(bundle);
	}
	else if(0 == strcmp("/1.ts", path))
	{
		StdCFile file("e:\\2.ts", "rb");
		long sz = file.GetFileSize();
		void* bundle = http_bundle_alloc(sz);
		void* ptr = http_bundle_lock(bundle);
		file.Read(ptr, sz);
		http_bundle_unlock(bundle, sz);

		//socket_send_all_by_time(sock, p, sz, 0, 5000);
		http_server_set_content_type(session, "video/mp2t");
		http_server_send(session, 200, bundle);
		http_bundle_free(bundle);
	}
	else
	{
		StdCFile file("e:\\2.ts", "rb");
		long sz = file.GetFileSize();
		void* bundle = http_bundle_alloc(sz);
		void* ptr = http_bundle_lock(bundle);
		file.Read(ptr, sz);
		http_bundle_unlock(bundle, sz);

		//socket_send_all_by_time(sock, p, sz, 0, 5000);
		http_server_set_content_type(session, "video/mp2t");
		http_server_send(session, 200, bundle);
		http_bundle_free(bundle);

		//assert(0);
	}

	return 0;
}

static void OnReadFile(void* camera, const void* data, int bytes)
{
	//static int64_t pcr = 0;
	//static int64_t pts = 0;

	//if(0 == pcr)
	//{
	//	time_t t;
	//	time(&t);
	//	pts = 412536249;//t * 90000;
	//}
	//else
	//{
	//	pts += 90 * 40; // 90kHZ * 40ms
	//}

	//pcr = pts * 300;
	hsl_server_input(camera, data, bytes, 0x1b);
}

static int STDCALL OnLiveThread(IN void* param)
{
	while(1)
	{
		H264Reader reader;
		reader.Open("e:\\sjz.h264");

		while(reader.Read(OnReadFile, param) > 0)
			Sleep(40);
	}

	return 0;
}

static int OnLiveOpen(void* param, void* camera, const char* id, const char* key, const char* publicId)
{
	thread_t thread;
	thread_create(&thread, OnLiveThread, camera);
	return 0;
}

static int OnLiveClose(void* param, const char* id)
{
	return 0;
}

BOOL CrtspclientDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	socket_init();
	rtp_avp_init();
//	aio_socket_init(1);
	
//	thread_t thread;
//	thread_create(&thread, OnThread, NULL);

	hls_server_init();
	void* hls = hls_server_create(NULL, 80);
	hsl_server_set_handle(hls, OnLiveOpen, OnLiveClose, hls);

	//http_server_init();
	//void* server = http_server_create(NULL, 80);
	//http_server_set_handler(server, OnHTTP, NULL);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CrtspclientDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CrtspclientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CrtspclientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static void OnData(void* param, unsigned char nal, const void* data, int bytes)
{
	FILE *fp = (FILE*)param;
	int startcode = 0x01000000;
	fwrite(&startcode, 1, 4, fp);
	fwrite(&nal, 1, 1, fp);
	fwrite(data, 1, bytes, fp);
	fflush(fp);
}

extern "C" int ts_packet_dec(const unsigned char* data, int bytes);

void CrtspclientDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	//OnOK();

#if 0
	unsigned char buffer[188] = {0};
	FILE *fp = fopen("e:\\2.ts", "rb");
	while(fread(buffer, 188, 1, fp) > 0)
	{
		ts_packet_dec(buffer, 188);
	}
	fclose(fp);
#endif


	//int rtp, rtcp;
	////void* rtsp = rtsp_open("rtsp://127.0.0.1/sjz.264");
	//void* rtsp = rtsp_open("rtsp://192.168.11.229/sjz.264");
	//rtsp_media_get_rtp_socket(rtsp, 0, &rtp, &rtcp);
	//void *queue = rtp_queue_create();
	//void *avp = rtp_avp_udp_create(rtp, rtcp, queue);
	//rtp_avp_udp_start(avp);
	//rtsp_play(rtsp);

	//FILE *fp = fopen("e:\\r.bin", "wb");
	//void* h264 = h264_source_create(queue, OnData, fp);
	//while(1)
	//{
	//	h264_source_process(h264);
	//}
	//h264_source_destroy(h264);
	//fclose(fp);
}

void CrtspclientDlg::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	//OnCancel();

#if 0
	H264Reader reader;
	reader.Open("e:\\sjz.h264");

	FILE *fp = fopen("e:\\2.ts", "wb");
	void* ts = mpeg_ts_create(OnTSWrite, fp);
	while(reader.Read(OnReadH264, ts) > 0);
	mpeg_ts_destroy(ts);
	fclose(fp);
#endif
}
