#ifndef untLMFSH
#define untLMFSH

#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Contnrs.hpp>
#include <Graphics.hpp>
#include <Grids.hpp>
#include <Dialogs.hpp>
#include <pngimage.hpp>
#include <vector>
#include <map>
#include <IniFiles.hpp>/

//----------------------------------------------------------
struct TFlowRow
{
	int GSeq;
	int Idx;
	String BfPnu;
	String AfPnu;
	String Rsn;
	String RegDt;
	String BfJimok;
	double BfArea;
	String AfJimok;
	double AfArea;

	String BfJibun;
	String AfJibun;
	String BfJimokName;
	String AfJimokName;
};

//----------------------------------------------------------
enum TNodeKind
{
	nkMainParcel,   // 검색지번(FMainPnu)
	nkSubParcel,    // 그 외 지번
	nkEvent         // 종목+일자 / 본번 / 분번 등 중간 라벨 박스
};

//----------------------------------------------------------
class TParcelNode : public TObject
{
public:
	String Key;       // depth|pnu
	String Pnu;
	String Caption;
	String Attr;
	int Depth;        // column
	int Lane;         // row
	bool IsMain;
	bool Selected;
	TRect Rect;
	TNodeKind NodeKind;

	__fastcall TParcelNode()
	{
		Key = L"";
		Pnu = L"";
		Caption = L"";
		Attr = L"";
		Depth = 0;
		Lane = 0;
		IsMain = false;
		Selected = false;
		Rect = Classes::Rect(0, 0, 0, 0);
	}
};

//----------------------------------------------------------
class TDiagramLink : public TObject
{
public:
    TParcelNode *FromNode;
    TParcelNode *ToNode;
	//String LabelText;
    String Rsn;
    String RegDt;
    String GroupKey;
	bool IsJump;
	bool IsMain;
	TNodeKind NodeKind;

    __fastcall TDiagramLink()
    {
		FromNode = NULL;
        ToNode = NULL;
        //LabelText = L"";
        Rsn = L"";
        RegDt = L"";
        GroupKey = L"";
        IsJump = false;
		IsMain = false;
	}
};

//----------------------------------------------------------
class TDepthLabel : public TObject
{
public:
	String Rsn;//이동사유
	String RegDt;//이동일자
    int Depth;
	TRect Rect;
    TNodeKind NodeKind;

    __fastcall TDepthLabel()
    {
		Rsn = L"";
		RegDt = L"";
        Depth = 0;
        Rect = Classes::Rect(0, 0, 0, 0);
    }
};

//----------------------------------------------------------
struct TNodeTheme
{
	TColor BgColor;
	TColor EdgeColor;
	TColor FontColor;
	bool Rounded;
};

//----------------------------------------------------------
//ZoomIn/ZoomOut//
struct TRenderContext
{
	TCanvas *Canvas;
	double Zoom;
	int OffsetX;
	int OffsetY;
	int BaseCornerRadius;
	bool DrawAttr;
};

//----------------------------------------------------------
class TfrmLMFS : public TForm
{
__published:
    TScrollBox *ScrollBox1;
    TPaintBox *PaintBox1;
	TPanel *pnlTool;
	TButton *btnSave;
	TCheckBox *chkAttr;
	TStringGrid *StringGrid1;
	TCheckBox *chkMain;
	TPanel *pnlTool2;
	TButton *btnDiagramSetting;
	TPanel *pnlTool1;
	TPanel *pnlDiagramSetting;
	TShape *shpEdge;
	TPanel *pnlDiagramSettingT;
	TButton *btnDiagramSettingClose;
	TPanel *Panel3;
	TColorDialog *ColorDialog1;
	TPanel *pnlSetting;
	TGroupBox *gbMain;
	TPanel *pnlColorMainEdge;
	TPanel *pnlColorMainFont;
	TPanel *pnlColorMainBg;
	TCheckBox *chkMainRound;
	TGroupBox *gbSub;
	TPanel *pnlColorSubEdge;
	TPanel *pnlColorSubFont;
	TPanel *pnlColorSubBg;
	TCheckBox *chkSubRound;
	TGroupBox *gbEvent;
	TPanel *pnlColorEventEdge;
	TPanel *pnlColorEventFont;
	TPanel *pnlColorEventBg;
	TCheckBox *chkEventRound;
	TPanel *pnlSettingTool;
	TButton *btnSettingSave;
	TImage *btnColorEventBg;
	TImage *btnColorEventEdge;
	TImage *btnColorEventFont;
	TImage *btnColorMainBg;
	TImage *btnColorMainEdge;
	TImage *btnColorMainFont;
	TImage *btnColorSubBg;
	TImage *btnColorSubEdge;
	TImage *btnColorSubFont;
	TSaveDialog *SaveDialog1;

    void __fastcall FormCreate(TObject *Sender);
	void __fastcall FormDestroy(TObject *Sender);
	void __fastcall FormShow(TObject *Sender);
    void __fastcall PaintBox1Paint(TObject *Sender);
    void __fastcall PaintBox1MouseDown(TObject *Sender, TMouseButton Button,
		TShiftState Shift, int X, int Y);
    void __fastcall PaintBox1MouseMove(TObject *Sender, TShiftState Shift,
        int X, int Y);
    void __fastcall PaintBox1MouseUp(TObject *Sender, TMouseButton Button,
        TShiftState Shift, int X, int Y);
	void __fastcall chkAttrClick(TObject *Sender);
	void __fastcall StringGrid1SelectCell(TObject *Sender, int ACol, int ARow, bool &CanSelect);
	void __fastcall btnSaveClick(TObject *Sender);
	void __fastcall StringGrid1DrawCell(TObject *Sender, int ACol, int ARow, TRect &Rect,
          TGridDrawState State);
	void __fastcall chkMainClick(TObject *Sender);
	void __fastcall gbColorClick(TObject *Sender);
	void __fastcall btnDiagramSettingClick(TObject *Sender);
	void __fastcall btnDiagramSettingCloseClick(TObject *Sender);
	void __fastcall btnSettingSaveClick(TObject *Sender);
	void __fastcall pnlColorClick(TObject *Sender);
	void __fastcall FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta,
          TPoint &MousePos, bool &Handled);


private:

	//===========================================================================
	// ArchiveLMFS에서 넘겨받은 검색 지번, PNU를 처리//
	//===========================================================================
	void __fastcall WMCopyData(TWMCopyData &Msg);
	BEGIN_MESSAGE_MAP
		MESSAGE_HANDLER(WM_COPYDATA, TWMCopyData, WMCopyData)
	END_MESSAGE_MAP(TForm)


	//----------------------------------------------------------
	AnsiString  DOWNLOADPATH;

    TObjectList *FNodes;
    TObjectList *FLinks;
    TObjectList *FLabels;

	TStringList *FNodeMap;      // key=depth|pnu, value=node index
    TStringList *FLaneMap;      // key=pnu, value=lane index
    TStringList *FDepthMap;     // key=regdt|idx|seq, value=depth index
	TStringList *FPnuCaptionCache;   // key=PNU19, value=완성 캡션

    bool FDragging;
    int FDragNodeIndex;
    TPoint FDragOffset;

    String FMainPnu;            // 검색 메인 PNU
	String FMainJibun;          // 검색 메인 JIBUN
	std::vector<TFlowRow> FRows;


	//----------------------------------------------------------
	// 다이어그램 노드, Level 박스 크기 등//
	//----------------------------------------------------------
	int NodeW;
//	int NodeH1;
//	int NodeH2;
	int LeftM;
//	int TopM;
	int GapX;
//	int GapY;
//	int LabelW;
//	int LabelH;
//	int RightM;
//	int BottomM;
	void __fastcall funcSetNodeBox();


	//해당되는 PNU의 DB Query 레코드 담긴 파일 읽기(LandArchive에서 생성 후 호출)//
	void __fastcall LoadDiagramData();
	bool __fastcall LoadFlowCsv(const String &AFileName, std::vector<TFlowRow> &FRows);
	void __fastcall ResetDiagram();

    String __fastcall MakeNodeKey(int ADepth, const String &APnu);
	String __fastcall MakeCaption(const String &APnu);
    String __fastcall MakeAttr(const TFlowRow &R, bool AUseAfter);
    String __fastcall MakeRsnText(const String &ARsn);

	int __fastcall GetLane(const String &APnu);
	String __fastcall GetDepthLabelText(TDepthLabel *L);
	int __fastcall AddDepth(const String &AKey);
	TParcelNode* __fastcall AddOrGetNode(int ADepth, const String &APnu,
        const String &AJibun, const String &AAttr);
	void __fastcall AddLink(TParcelNode *AFrom, TParcelNode *ATo,
        /*const String &ALabelText, */const String &ARsn, const String &ARegDt);
	void __fastcall AddDepthLabel(const String &ARsnText, const String &ARegDtText, int ADepth);

    bool __fastcall NeedsJump(TDiagramLink *A, TDiagramLink *B);
	void __fastcall ResolveJumpFlags();

	TParcelNode* __fastcall FindLatestNodeBeforeDepth(const String &APnu, int ADepth);
	void __fastcall AnalyzeRowsToDiagram(const std::vector<TFlowRow> &FRows);
	//---------------------------------------------------------------------------
	//---------------------------------------------------------------------------
	void __fastcall RefreshDiagram();
	void __fastcall BuildLayout();
    void __fastcall UpdateCanvasSize();

	void __fastcall DrawNode(const TRenderContext &RC, TParcelNode *N);
	void __fastcall DrawLink(const TRenderContext &RC, TDiagramLink *L);
	void __fastcall DrawDepthLabel(const TRenderContext &RC, TDepthLabel *L);
	void __fastcall DrawArrow(const TRenderContext &RC, int x1, int y1, int x2, int y2);
    void __fastcall DrawJumpArc(const TRenderContext &RC, int X, int Y);
    int __fastcall HitTestNode(int X, int Y);
    
	//---------------------------------------------------------
	// 시도코드 관련//
	//---------------------------------------------------------
	TStringList *FSidoCodeMap;   // key=10자리 법정동코드, value=지역명
	String FLastAreaCode10;
	String FLastAreaName;
	bool __fastcall LoadSidoCodeCsv(const String &AFileName);
	String __fastcall GetAreaNameByCode10(const String &ACode10);
	String __fastcall MakeJibunText(const String &APnu);
	String __fastcall GetOrCreateJibun(const String &APnu);

	//---------------------------------------------------------
	// 지목코드 관련//
	//---------------------------------------------------------
	TStringList *FJimokMap;   // "05=임야"
	bool __fastcall LoadJimokCsv(const String &AFileName);
	String __fastcall GetJimokName(const String &ACode);

	//---------------------------------------------------------
	// TCanvas PNG 저장
	//---------------------------------------------------------
	void __fastcall SaveDiagramToPng(const String &AFileName);
	//---------------------------------------------------------
	// TCanvas PDF (Micorosoft to PDF) 저장
	//---------------------------------------------------------
	//void __fastcall SaveDiagramToPdf(const String &AFileName);

	//---------------------------------------------------------
	// LandArchive로 검색지번 보내기
	//---------------------------------------------------------
	void __fastcall SendPnuToLandArchive(const String &APnu, const String &AJibun);

	//---------------------------------------------------------------------------
	// 지번목록 Grid//
	// Unique 지번목록(한글 순) 표시 (특정 지번 선택 다시 그리기용)//
	//---------------------------------------------------------
	void __fastcall AddJibunToGridMap(TStringList *AMap, const String &APnu, const String &AJibun);
	void __fastcall CreateGrid();
	void __fastcall InitGrid();
	void __fastcall DisplayGrid(const std::vector<TFlowRow> &FRows);


	//---------------------------------------------------------------------------
	// YYYYMMDD => YYYY-MM-DD
	//---------------------------------------------------------------------------
	AnsiString __fastcall funcChangeDateFormatString(AnsiString _asDateStr);

	//---------------------------------------------------------------------------
	// chkAll - [ 연관필지 전체 ] 표시
	//---------------------------------------------------------------------------
	TStringList *FMainPnuMap;

	//---------------------------------------------------------------------------
	// [다이어그램 환경설정]
	//---------------------------------------------------------------------------
	TNodeTheme FThemeMainNode;   // 검색지번(FMainPnu)
	TNodeTheme FThemeSubNode;    // 외지번
	TNodeTheme FThemeEventNode;  // 종목+일자
	TNodeTheme oldMain;
	TNodeTheme oldSub;
	TNodeTheme oldEvent;
	void __fastcall SetDefaultThemes();
	void __fastcall LoadThemesFromIni();
	void __fastcall SaveThemesToIni();
	const TNodeTheme& __fastcall GetThemeForNode(TParcelNode *N);
	const TNodeTheme& __fastcall GetThemeByKind(TNodeKind AKind);
	void __fastcall ApplyToOwner();


	//---------------------------------------------------------------------------
	// ZoomIn/ZoomOut
	//---------------------------------------------------------------------------
	double FZoom;
	int FOffsetX;
	int FOffsetY;
	int FDiagramWidth;
	int FDiagramHeight;
	void __fastcall RenderDiagram(TCanvas *C, double AZoom,
		int AOffsetX, int AOffsetY, bool ADrawAttr);
	int __fastcall SX(int X, const TRenderContext &RC) const;
	int __fastcall SY(int Y, const TRenderContext &RC) const;
	TRect __fastcall ScaleRect(const TRect &R, const TRenderContext &RC) const;
	int __fastcall ScaleValue(int V, const TRenderContext &RC, int AMin) const;
	int __fastcall ScaleFont(int baseSize, const TRenderContext &RC,
		int minSizeScreen, int minSizePrint, int maxSize) const;
	int __fastcall IRound(double x) const;
	void __fastcall PrepareTextStyle(TCanvas *C, TColor ATextColor);


	//---------------------------------------------------------------------------
	// Microsoft to PDF
	//---------------------------------------------------------------------------
	void __fastcall ExportToPdfByPrinter();
	//bool __fastcall PrintToPdfDirect(const String &APdfFileName);


public:
    __fastcall TfrmLMFS(TComponent* Owner);
};

extern PACKAGE TfrmLMFS *frmLMFS;

#endif
