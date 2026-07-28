//===========================================================
// 1. CSV 읽기 (추후, DB에서 읽을 예)
// 2. PNU별 row 배정
// 3. 각 행에서 bf_pnu, af_pnu에 대한 노드 생성
// 4. 같은 PNU면 같은 row로 고정
// 5. 링크 생성
// 6. 링크가 다른 row를 넘으면 IsJump = true
// 7. PaintBox에서 main PNU 강조, jump 링크 표시, 나머지 일반색 처리
//===========================================================

#include <vcl.h>
#pragma hdrstop

#include "untLMFS.h"
#include <math.h>
#include <map>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#include <PNGImage.hpp>
#include <Windows.hpp>//WM_COPYDATA

#include <IniFiles.hpp>//ini 읽기

//for PDF Printer
#include <Printers.hpp>
#include <SysUtils.hpp>
#include <Windows.hpp>

#pragma package(smart_init)
#pragma resource "*.dfm"

//----------------------------------------------------------
AnsiString  DEBUGPNU = "4471025021100710001";

//----------------------------------------------------------
TfrmLMFS *frmLMFS;

//----------------------------------------------------------
__fastcall TfrmLMFS::TfrmLMFS(TComponent* Owner)
    : TForm(Owner)
{
    FNodes = NULL;
	FLinks = NULL;
    FLabels = NULL;
    FNodeMap = NULL;
    FLaneMap = NULL;
    FDepthMap = NULL;

    FDragging = false;
    FDragNodeIndex = -1;
    FDragOffset = Point(0, 0);

	//PNG 저장 폴더(DOWNLOAD) 생성하기//
	DOWNLOADPATH    = ExtractFilePath(Application->ExeName) + "DOWNLOAD\\";
	if (!DirectoryExists(DOWNLOADPATH))
		CreateDir(DOWNLOADPATH);
}

//----------------------------------------------------------
void __fastcall TfrmLMFS::FormCreate(TObject *Sender)
{
    DoubleBuffered = true;
    ScrollBox1->DoubleBuffered = true;
    PaintBox1->ControlStyle = PaintBox1->ControlStyle << csOpaque;

    FNodes = new TObjectList(true);
    FLinks = new TObjectList(true);
    FLabels = new TObjectList(true);

    FNodeMap = new TStringList();
    FNodeMap->NameValueSeparator = '=';
    FNodeMap->StrictDelimiter = true;

    FLaneMap = new TStringList();
    FLaneMap->NameValueSeparator = '=';
    FLaneMap->StrictDelimiter = true;

    FDepthMap = new TStringList();
    FDepthMap->NameValueSeparator = '=';
    FDepthMap->StrictDelimiter = true;
    
    FPnuCaptionCache = new TStringList();
    FPnuCaptionCache->NameValueSeparator = '=';
    FPnuCaptionCache->StrictDelimiter = true;

	//시도코드 가져오기//
	FSidoCodeMap = new TStringList();
    FSidoCodeMap->NameValueSeparator = '=';
	FSidoCodeMap->StrictDelimiter = true;
    FLastAreaCode10 = L"";
	FLastAreaName = L"";
	//시도코드 읽기(해당 시군구 코드만 갖고 있기)
	LoadSidoCodeCsv(ExtractFilePath(Application->ExeName) + L"DB\\SIDOCODE.csv");

	//지목 28종 가져오기
    FJimokMap = new TStringList();
    FJimokMap->NameValueSeparator = '=';
	LoadJimokCsv(ExtractFilePath(Application->ExeName) + L"DB\\JIMOK.csv");

	//검색지번과 직접 연관 필지목록만 (대표이사REQ)
	FMainPnuMap = new TStringList();


	//지번목록 구성 초기화//
	CreateGrid();

	//INI 읽기 - 다이어그램 설정정보//
	LoadThemesFromIni();
	//다이어그램 설정 화면 위치 Setting//
	pnlDiagramSetting->Left = (this->Width - pnlDiagramSetting->Width)/2;
	pnlDiagramSetting->Top = (this->Height - pnlDiagramSetting->Height)/2;


	//---------------------------------------------------------------------------
	// ZoomIn/ZoomOut
	//---------------------------------------------------------------------------
	FZoom = 1.0;
	FOffsetX = 20;
	FOffsetY = 20;
	FDiagramWidth = 1400;
	FDiagramHeight = 860;

	//----------------------------------------------------------
	// 다이어그램 노드, Level 박스 크기 등//
	//----------------------------------------------------------
	funcSetNodeBox();
}

//----------------------------------------------------------
void __fastcall TfrmLMFS::FormDestroy(TObject *Sender)
{
	delete FMainPnuMap;
	FMainPnuMap = NULL;
	delete FJimokMap;
	FJimokMap = NULL;
	delete FSidoCodeMap;
	FSidoCodeMap = NULL;
	delete FPnuCaptionCache;
	FPnuCaptionCache = NULL;
	delete FDepthMap;
	FDepthMap = NULL;
	delete FLaneMap;
	FLaneMap = NULL;
	delete FNodeMap;
	FNodeMap = NULL;
	delete FLabels;
	FLabels = NULL;
	delete FLinks;
	FLinks = NULL;
	delete FNodes;
	FNodes = NULL;
}

//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::FormShow(TObject *Sender)
{
	//인수로 들어온 PNU 확인//
	int paramCount = ParamCount();
	if(paramCount == 1)
		FMainPnu = ParamStr(1);
	else
		FMainPnu = DEBUGPNU;

	LoadDiagramData();

	ScrollBox1->SetFocus();
}


//----------------------------------------------------------
// 다이어그램 노드, Level 박스 크기 등//
//----------------------------------------------------------
void __fastcall TfrmLMFS::funcSetNodeBox()
{
	NodeW = 140;
//	NodeH1 = 34;
//	NodeH2 = 56;
	LeftM = 40;
//	TopM  = 40;
	GapX  = 280;
//	GapY  = 20;
//	LabelW  = NodeW - 10;
//	LabelH  = NodeH1;
//	RightM  = 80;
//	BottomM = 80;
}


//---------------------------------------------------------------------------
//해당되는 PNU의 DB Query 레코드 담긴 파일 읽기(LandArchive에서 생성 후 호출)//
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::LoadDiagramData()
{
	String fileName = ExtractFilePath(Application->ExeName) + L"DB\\" + FMainPnu + ".csv";

	FRows.clear();

	//CSV 파일 읽기 > 파싱하기//
	if (LoadFlowCsv(fileName, FRows))
	{
		DisplayGrid(FRows);//지번목록 표시//
		AnalyzeRowsToDiagram(FRows);
		RefreshDiagram();//다이어그램 그리기//
	}
	else
	{
		String  sMsg = "";
		sMsg.sprintf(L"CSV 파일을 읽지 못했습니다.\n%s", fileName);
		Application->MessageBox(sMsg.c_str(), L"알림", MB_OK);
	}
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::ResetDiagram()
{
    FNodes->Clear();
    FLinks->Clear();
    FLabels->Clear();

    FNodeMap->Clear();
    FLaneMap->Clear();
    FDepthMap->Clear();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool __fastcall TfrmLMFS::LoadFlowCsv(const String &AFileName, std::vector<TFlowRow> &FRows)
{
	if (!FileExists(AFileName))
        return false;

    TStringList *lines = new TStringList();
    TStringList *cols = new TStringList();

    try
    {
        lines->LoadFromFile(AFileName);

        cols->Delimiter = ',';
        cols->StrictDelimiter = true;

        for (int i = 1; i < lines->Count; i++) // header 제외
        {
            String line = Trim(lines->Strings[i]);
            if (line.IsEmpty()) continue;

            cols->DelimitedText = line;
			if (cols->Count < 10) continue;

            TFlowRow R;
            R.GSeq    = StrToIntDef(Trim(cols->Strings[0]), 0);
            R.Idx     = StrToIntDef(Trim(cols->Strings[1]), 0);
            R.BfPnu   = Trim(cols->Strings[2]);
            R.AfPnu   = Trim(cols->Strings[3]);
            R.Rsn     = Trim(cols->Strings[4]);
            R.RegDt   = Trim(cols->Strings[5]);
            R.BfJimok = Trim(cols->Strings[6]);
            R.BfArea  = StrToFloatDef(Trim(cols->Strings[7]), 0);
            R.AfJimok = Trim(cols->Strings[8]);
			R.AfArea  = StrToFloatDef(Trim(cols->Strings[9]), 0);

			//코드 => 명칭 변경
			R.BfJibun = GetOrCreateJibun(R.BfPnu);
			R.AfJibun = GetOrCreateJibun(R.AfPnu);
			R.BfJimokName = GetJimokName(R.BfJimok);
			R.AfJimokName = GetJimokName(R.AfJimok);

			FRows.push_back(R);
        }
    }
    __finally
    {
        delete cols;
        delete lines;
    }

	return !FRows.empty();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::MakeNodeKey(int ADepth, const String &APnu)
{
    return IntToStr(ADepth) + L"|" + APnu;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::MakeCaption(const String &APnu)
{
	//매번 PNU를 변환하니 1번 변환항 PNU 다시 않도록 변경//
    int idx = FPnuCaptionCache->IndexOfName(APnu);
    if (idx >= 0)
        return FPnuCaptionCache->ValueFromIndex[idx];

    String caption = MakeJibunText(APnu);
    FPnuCaptionCache->Values[APnu] = caption;
    return caption;
    //return MakeJibunText(APnu);
   	// 우선 PNU 그대로 표시. 이후 지번 변환 함수로 교체 가능.
    //return APnu;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::MakeAttr(const TFlowRow &R, bool AUseAfter)
{
    if (AUseAfter)
		return L"[" + R.AfJimokName + L" " + FloatToStr(R.AfArea) + L"]";
	else
        return L"[" + R.BfJimokName + L" " + FloatToStr(R.BfArea) + L"]";
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::MakeRsnText(const String &ARsn)
{
	if (ARsn == L"10") return L"등록전환";
    if (ARsn == L"20") return L"분할";
    if (ARsn == L"30") return L"합병";
    if (ARsn == L"40") return L"지목변경";
    if (ARsn == L"70") return L"기타";
    return ARsn;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::GetLane(const String &APnu)
{
    int idx = FLaneMap->IndexOfName(APnu);
    if (idx >= 0)
        return StrToIntDef(FLaneMap->ValueFromIndex[idx], 0);

    int lane = FLaneMap->Count;
    FLaneMap->Values[APnu] = IntToStr(lane);
    return lane;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::GetDepthLabelText(TDepthLabel *L)
{
    String s = Trim(L->Rsn);
    String d = Trim(L->RegDt);

    if (!s.IsEmpty() && !d.IsEmpty())
        return s + L"\r\n" + d;
    else if (!s.IsEmpty())
        return s;
    else
        return d;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::AddDepth(const String &AKey)
{
    int idx = FDepthMap->IndexOfName(AKey);
    if (idx >= 0)
        return StrToIntDef(FDepthMap->ValueFromIndex[idx], 0);

    int depth = FDepthMap->Count;
    FDepthMap->Values[AKey] = IntToStr(depth);
    return depth;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
TParcelNode* __fastcall TfrmLMFS::AddOrGetNode(int ADepth, const String &APnu,
	const String &AJibun, const String &AAttr)
{
    String key = MakeNodeKey(ADepth, APnu);

    int mapIdx = FNodeMap->IndexOfName(key);
    if (mapIdx >= 0)
	{
        int nodeIdx = StrToIntDef(FNodeMap->ValueFromIndex[mapIdx], -1);
        if (nodeIdx >= 0 && nodeIdx < FNodes->Count)
        {
            TParcelNode *N = (TParcelNode*)FNodes->Items[nodeIdx];
            if (!AAttr.IsEmpty()) N->Attr = AAttr;
			return N;
        }
    }

	TParcelNode *N = new TParcelNode();
    N->Key = key;
	N->Pnu = APnu;
	N->Caption = AJibun;//MakeCaption(APnu);
    N->Attr = AAttr;
    N->Depth = ADepth;
    N->Lane = GetLane(APnu);
	N->IsMain = APnu == FMainPnu;
	N->NodeKind = N->IsMain ? nkMainParcel : nkSubParcel;
	if(N->IsMain)
		FMainJibun = N->Caption;//다이어그램 저장 시 [지번]으로 저장하기 위함//

	int newIndex = FNodes->Add(N);
	FNodeMap->Values[key] = IntToStr(newIndex);

    return N;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::AddLink(TParcelNode *AFrom, TParcelNode *ATo,
	/*const String &ALabelText, */const String &ARsn, const String &ARegDt)
{
    if (!AFrom || !ATo) return;

	TDiagramLink *L = new TDiagramLink();
	L->FromNode = AFrom;
	L->ToNode = ATo;
	//L->LabelText = ALabelText;
    L->Rsn = ARsn;
    L->RegDt = ARegDt;
	L->GroupKey = ARsn + L"|" + ARegDt;
	L->IsJump = false;
	L->IsMain = (FMainPnu == AFrom->Pnu || FMainPnu == ATo->Pnu) ? (true):(false);

    FLinks->Add(L);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::AddDepthLabel(const String &ARsnText, const String &ARegDtText, int ADepth)
{
    for (int i = 0; i < FLabels->Count; i++)
    {
        TDepthLabel *L = (TDepthLabel*)FLabels->Items[i];
        if (L->Depth == ADepth) return;
    }

	TDepthLabel *L = new TDepthLabel();
	L->Rsn = ARsnText;
	L->RegDt = ARegDtText;
	L->Depth = ADepth;
	L->NodeKind = nkEvent;
    FLabels->Add(L);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool __fastcall TfrmLMFS::NeedsJump(TDiagramLink *A, TDiagramLink *B)
{
    if (!A || !B || !A->FromNode || !A->ToNode || !B->FromNode || !B->ToNode)
        return false;

    int ax1 = A->FromNode->Rect.right;
    int ay1 = (A->FromNode->Rect.top + A->FromNode->Rect.bottom) / 2;
    int ax2 = A->ToNode->Rect.left;
    int ay2 = (A->ToNode->Rect.top + A->ToNode->Rect.bottom) / 2;
    int amid = (ax1 + ax2) / 2;

    int bx1 = B->FromNode->Rect.right;
    int by1 = (B->FromNode->Rect.top + B->FromNode->Rect.bottom) / 2;
    int bx2 = B->ToNode->Rect.left;
	int by2 = (B->ToNode->Rect.top + B->ToNode->Rect.bottom) / 2;
    int bmid = (bx1 + bx2) / 2;

	int bLeft   = (bx1 < bx2) ? bx1 : bx2;
	int bRight  = (bx1 > bx2) ? bx1 : bx2;
	int aTop    = (ay1 < ay2) ? ay1 : ay2;
	int aBottom = (ay1 > ay2) ? ay1 : ay2;

	int aLeft   = (ax1 < ax2) ? ax1 : ax2;
	int aRight  = (ax1 > ax2) ? ax1 : ax2;
	int bTop    = (by1 < by2) ? by1 : by2;
	int bBottom = (by1 > by2) ? by1 : by2;

	bool aVerticalHitsBHorizontal =
		(amid > bLeft && amid < bRight) &&
		(by1 > aTop && by1 < aBottom);

	bool bVerticalHitsAHorizontal =
		(bmid > aLeft && bmid < aRight) &&
		(ay1 > bTop && ay1 < bBottom);

	return aVerticalHitsBHorizontal || bVerticalHitsAHorizontal;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::ResolveJumpFlags()
{
    for (int i = 0; i < FLinks->Count; i++)
    {
        TDiagramLink *A = (TDiagramLink*)FLinks->Items[i];
        A->IsJump = false;

        for (int j = 0; j < i; j++)
        {
			TDiagramLink *B = (TDiagramLink*)FLinks->Items[j];

            if (A->GroupKey == B->GroupKey)
                continue; // 같은 이벤트 그룹은 jump 안 함

            if (NeedsJump(A, B))
            {
                A->IsJump = true;
                break;
            }
        }
    }
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
TParcelNode* __fastcall TfrmLMFS::FindLatestNodeBeforeDepth(const String &APnu, int ADepth)
{
	TParcelNode *best = NULL;

	for (int i = 0; i < FNodes->Count; i++)
	{
		TParcelNode *N = (TParcelNode*)FNodes->Items[i];
		if (N->Pnu != APnu)
			continue;
		if (N->Depth >= ADepth)
			continue;

		if (!best || N->Depth > best->Depth)
			best = N;
	}

	return best;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::AnalyzeRowsToDiagram(const std::vector<TFlowRow> &FRows)
{
    ResetDiagram();

	for (size_t i = 0; i < FRows.size(); i++)
    {
		const TFlowRow &R = FRows[i];

		//[대표이사REQ]
		if( !chkMain->Checked || (FMainPnu == R.BfPnu || FMainPnu == R.AfPnu))
		{
			if (FMainPnuMap->IndexOf(R.BfPnu) == -1)
				FMainPnuMap->Add(R.BfPnu);
			if (FMainPnuMap->IndexOf(R.AfPnu) == -1)
				FMainPnuMap->Add(R.AfPnu);

			// 각 이벤트를 고유 depth로 둠: reg_dt + idx + seq
			//String depthKey = R.RegDt + L"|" + FormatFloat(L"000000", R.Idx) + L"|" + IntToStr((int)i);
			//int depth = AddDepth(depthKey);
			String eventKey = R.Rsn + L"|" + R.RegDt;
			int depth = AddDepth(eventKey);

			String rsnName = MakeRsnText(R.Rsn);
			String label   = rsnName + " " + funcChangeDateFormatString(R.RegDt);

			AddDepthLabel(rsnName, funcChangeDateFormatString(R.RegDt), depth);

			//
//			TParcelNode *fromNode = AddOrGetNode(depth, R.BfPnu, R.BfJibun, MakeAttr(R, false));
//			TParcelNode *toNode   = AddOrGetNode(depth + 1, R.AfPnu, R.AfJibun, MakeAttr(R, true));
			//from(이동전) 노드//
			TParcelNode *fromNode = FindLatestNodeBeforeDepth(R.BfPnu, depth);
			// 이전에 그려진 같은 PNU 노드가 없을 때만 새로 생성
			if (!fromNode)
				fromNode = AddOrGetNode(depth, R.BfPnu, R.BfJibun, MakeAttr(R, false));
			//to(이동후) 노드//
			TParcelNode *toNode = AddOrGetNode(depth + 1, R.AfPnu, R.AfJibun, MakeAttr(R, true));

			AddLink(fromNode, toNode, /*label, */rsnName, R.RegDt);
		}

		//260713//ORG//
//		TParcelNode *fromNode = AddOrGetNode(depth, R.BfPnu, R.BfJibun, MakeAttr(R, false));
//		TParcelNode *toNode   = AddOrGetNode(depth + 1, R.AfPnu, R.AfJibun, MakeAttr(R, true));
//
//		AddLink(fromNode, toNode, label, rsnName, R.RegDt);
	}
}

//---------------------------------------------------------------------------
// 다이어그램 다시 그리기//
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::RefreshDiagram()
{
	BuildLayout();			// 노드 Rect 확정//
	ResolveJumpFlags();		// Rect 기반 교차 판정//
	PaintBox1->Invalidate();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::BuildLayout()
{
//	const int NodeW = 140;
	const int NodeH1 = 34;
	const int NodeH2 = 56;
//	const int LeftM = 40;
	const int TopM  = 40;
//	const int GapX  = 280;
	const int GapY  = 20;
    const int LabelW  = NodeW - 10;
    const int LabelH  = NodeH1;
    const int RightM  = 80;
    const int BottomM = 80;

	bool showAttr = chkAttr->Checked;
    int nodeH = showAttr ? NodeH2 : NodeH1;

    int maxRight = 0;
    int maxBottom = 0;

	for (int i = 0; i < FNodes->Count; i++)
	{
		TParcelNode *N = (TParcelNode*)FNodes->Items[i];

		int x = LeftM + N->Depth * GapX;
		int y = TopM + N->Lane * (nodeH + GapY);

		N->Rect = Classes::Rect(x, y, x + NodeW, y + nodeH);

        if (N->Rect.right > maxRight)
			maxRight = N->Rect.right;
        if (N->Rect.bottom > maxBottom)
            maxBottom = N->Rect.bottom;
	}

	for (int i = 0; i < FLabels->Count; i++)
	{
		TDepthLabel *L = (TDepthLabel*)FLabels->Items[i];

		int x = LeftM + L->Depth * GapX + NodeW + 5;//10;
		int y = 14;

        L->Rect = Classes::Rect(x, y, x + LabelW, y + LabelH);

        if (L->Rect.right > maxRight)
            maxRight = L->Rect.right;
        if (L->Rect.bottom > maxBottom)
            maxBottom = L->Rect.bottom;
    }

    FDiagramWidth = maxRight + RightM;
    FDiagramHeight = maxBottom + BottomM;

    if (FDiagramWidth < ScrollBox1->ClientWidth)
        FDiagramWidth = ScrollBox1->ClientWidth;

    if (FDiagramHeight < ScrollBox1->ClientHeight)
        FDiagramHeight = ScrollBox1->ClientHeight;

    UpdateCanvasSize();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::UpdateCanvasSize()
{
	// ZoomIn/ZoomOut
	PaintBox1->Width  = FOffsetX * 2 + int(IRound(FDiagramWidth * FZoom));
	PaintBox1->Height = FOffsetY * 2 + int(IRound(FDiagramHeight * FZoom));
/*
	int maxX = 1400;
	int maxY = 860;

	for (int i = 0; i < FNodes->Count; i++)
	{
		TParcelNode *N = (TParcelNode*)FNodes->Items[i];
		if (N->Rect.right + 120 > maxX) maxX = N->Rect.right + 120;
		if (N->Rect.bottom + 120 > maxY) maxY = N->Rect.bottom + 120;
	}

	PaintBox1->Width = maxX;
	PaintBox1->Height = maxY;
*/
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//void __fastcall TfrmLMFS::DrawArrow(TCanvas *C, int x1, int y1, int x2, int y2)
void __fastcall TfrmLMFS::DrawArrow(const TRenderContext &RC, int x1, int y1, int x2, int y2)
{
    TCanvas *C = RC.Canvas;

    int sx1 = SX(x1, RC);
    int sy1 = SY(y1, RC);
    int sx2 = SX(x2, RC);
    int sy2 = SY(y2, RC);

    double angle = atan2((double)(sy2 - sy1), (double)(sx2 - sx1));

    int len = ScaleValue(8, RC, 4);

    const double PI = 3.14159265358979323846;
    double a1 = angle + PI * 0.85;
    double a2 = angle - PI * 0.85;

	int ax1 = sx2 + (int)IRound(cos(a1) * len);
    int ay1 = sy2 + (int)IRound(sin(a1) * len);

    int ax2 = sx2 + (int)IRound(cos(a2) * len);
    int ay2 = sy2 + (int)IRound(sin(a2) * len);

    C->MoveTo(sx2, sy2);
    C->LineTo(ax1, ay1);

    C->MoveTo(sx2, sy2);
	C->LineTo(ax2, ay2);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//void __fastcall TfrmLMFS::DrawJumpArc(TCanvas *C, int X, int Y)
void __fastcall TfrmLMFS::DrawJumpArc(const TRenderContext &RC, int X, int Y)
{
    // 작은 반원 bump
//    C->Arc(X - 8, Y - 8, X + 8, Y + 8, X - 8, Y, X + 8, Y);
    TCanvas *C = RC.Canvas;

    int sx = SX(X, RC);
    int sy = SY(Y, RC);
    int r  = ScaleValue(8, RC, 4);

    C->Arc(
        sx - r, sy - r,
        sx + r, sy + r,
        sx - r, sy,
        sx + r, sy
	);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::DrawLink(const TRenderContext &RC, TDiagramLink *L)
{
    if (!L || !L->FromNode || !L->ToNode)
        return;

    TCanvas *C = RC.Canvas;
	TRect r1 = L->FromNode->Rect;
	TRect r2 = L->ToNode->Rect;

    L->IsMain = (FMainPnu == L->FromNode->Pnu || FMainPnu == L->ToNode->Pnu);

    int x1 = r1.right;
    int y1 = (r1.top + r1.bottom) / 2;
    int x2 = r2.left;
    int y2 = (r2.top + r2.bottom) / 2;
	int midX = (x1 + x2) / 2;

	if(L->IsMain)
	{
        C->Pen->Color = TColor(RGB(237, 125, 49));
        C->Pen->Width = ScaleValue(3, RC, 1);
	}
	else
	{
        C->Pen->Color = TColor(RGB(0, 0, 0));
        C->Pen->Width = ScaleValue(1, RC, 1);
	}

	C->Pen->Style = psSolid;

	//debug
/*	if((L->FromNode->Pnu == "4471025021100580004" && L->ToNode->Pnu == "4471025021100580004")
	|| (L->FromNode->Pnu == "4471025021100710010" && L->ToNode->Pnu == "4471025021100580004"))
	{
		int a = 1;
	}
*/

	// 같은 지번을 재사용해서 depth를 건너뛴 경우:
	// 출발 depth에서 바로 다음 depth 열까지 수평 이동한 후,
	// 그 지점에서 도착 노드 Y까지 수직으로 내려오고,
	// 마지막에 도착 노드로 수평 진입
	if (L->FromNode->Pnu == L->ToNode->Pnu || (L->ToNode->Depth - L->FromNode->Depth > 1))
	{
		//int bendDepth = L->FromNode->Depth + 1;
		//int bendX = LeftM + bendDepth * GapX;   // 다음 depth 열의 시작 X
		//my//int enterX = bendX - 10;                // 살짝 왼쪽에서 내려오게 조정 가능
		//my//int enterX = bendX - NodeW/2;                // 살짝 왼쪽에서 내려오게 조정 가능
		int enterX = x2 - NodeW/2;                // 살짝 왼쪽에서 내려오게 조정 가능

		C->MoveTo(SX(x1, RC), SY(y1, RC));
		C->LineTo(SX(enterX, RC), SY(y1, RC));  // 수평
		C->LineTo(SX(enterX, RC), SY(y2, RC));  // 수직
		C->LineTo(SX(x2, RC), SY(y2, RC));      // 도착 노드로 수평 진입

		DrawArrow(RC, enterX, y2, x2, y2);
		return;
	}
/*	// 같은 지번이면 수평 연결
	if (L->FromNode->Pnu == L->ToNode->Pnu)
	{
		int yy = (y1 + y2) / 2;

		C->MoveTo(SX(x1, RC), SY(yy, RC));
		C->LineTo(SX(x2, RC), SY(yy, RC));
		DrawArrow(RC, x1, yy, x2, yy);
		return;
	}
*/
/*    // 같은 PNU로 이어지는 유지 링크는 살짝 위로 올려서 표시
	if (L->FromNode->Pnu == L->ToNode->Pnu)
    {
        int raise = 18;
        int a = x1 + 20;
        int b = x2 - 20;

        if (b < a)
            b = a + 8;

        C->MoveTo(SX(x1, RC), SY(y1, RC));
        C->LineTo(SX(a, RC), SY(y1, RC));
        C->LineTo(SX(a, RC), SY(y1 - raise, RC));
        C->LineTo(SX(b, RC), SY(y2 - raise, RC));
        C->LineTo(SX(b, RC), SY(y2, RC));
        C->LineTo(SX(x2, RC), SY(y2, RC));

        DrawArrow(RC, b, y2, x2, y2);
        return;
    }
*/
	if (!L->IsJump)
    {
        C->MoveTo(SX(x1, RC), SY(y1, RC));
        C->LineTo(SX(midX, RC), SY(y1, RC));
        C->LineTo(SX(midX, RC), SY(y2, RC));
        C->LineTo(SX(x2, RC), SY(y2, RC));
    }
    else
    {
		int jumpX = midX;
		int jumpY = y2;
		int gap = ScaleValue(10, RC, 4);

		C->MoveTo(SX(x1, RC), SY(y1, RC));
		C->LineTo(SX(midX, RC) - gap, SY(y1, RC));
		C->LineTo(SX(midX, RC) - gap, SY(y2, RC));
		C->LineTo(SX(jumpX, RC) - gap, SY(jumpY, RC));

		DrawJumpArc(RC, jumpX, jumpY);

		C->MoveTo(SX(jumpX, RC) + ScaleValue(8, RC, 3), SY(jumpY, RC));
		C->LineTo(SX(x2, RC), SY(y2, RC));
	}

    DrawArrow(RC, midX, y2, x2, y2);
}
//void __fastcall TfrmLMFS::DrawLink(TCanvas *C, TDiagramLink *L)
/*void __fastcall TfrmLMFS::DrawLink(const TRenderContext &RC, TDiagramLink *L)
{
    if (!L || !L->FromNode || !L->ToNode)
        return;

    TCanvas *C = RC.Canvas;
    TRect r1 = L->FromNode->Rect;
    TRect r2 = L->ToNode->Rect;

    int x1 = r1.right;
    int y1 = (r1.top + r1.bottom) / 2;
    int x2 = r2.left;
    int y2 = (r2.top + r2.bottom) / 2;

    if (L->IsMain)
    {
        C->Pen->Color = TColor(RGB(237, 125, 49));
        C->Pen->Width = ScaleValue(3, RC, 1);
    }
    else
    {
        C->Pen->Color = clBlack;
        C->Pen->Width = ScaleValue(1, RC, 1);
    }

    C->Pen->Style = psSolid;

    // 자기 자신 유지 링크: 일반 링크와 다르게 위로 살짝 들어올려서 보이게 그림
    if (L->FromNode->Pnu == L->ToNode->Pnu)
    {
        int up = 16;
        int xA = x1 + 18;
        int xB = x2 - 18;

        if (xB < xA)
            xB = xA + 10;

        C->MoveTo(SX(x1, RC), SY(y1, RC));
        C->LineTo(SX(xA, RC), SY(y1, RC));
        C->LineTo(SX(xA, RC), SY(y1 - up, RC));
        C->LineTo(SX(xB, RC), SY(y2 - up, RC));
        C->LineTo(SX(xB, RC), SY(y2, RC));
        C->LineTo(SX(x2, RC), SY(y2, RC));

        DrawArrow(RC, xB, y2, x2, y2);
        return;
    }

    int turnX = x2 - 24;
    if (turnX < x1 + 20)
        turnX = x1 + 20;

    if (!L->IsJump)
    {
        C->MoveTo(SX(x1, RC), SY(y1, RC));
        C->LineTo(SX(turnX, RC), SY(y1, RC));
        C->LineTo(SX(turnX, RC), SY(y2, RC));
        C->LineTo(SX(x2, RC), SY(y2, RC));
    }
    else
    {
        int jumpX = turnX;
        int jumpY = y2;
        int gap = 10;

        C->MoveTo(SX(x1, RC), SY(y1, RC));
        C->LineTo(SX(jumpX - gap, RC), SY(y1, RC));
        C->LineTo(SX(jumpX - gap, RC), SY(y2, RC));
        DrawJumpArc(RC, jumpX, jumpY);
        C->MoveTo(SX(jumpX + 8, RC), SY(jumpY, RC));
        C->LineTo(SX(x2, RC), SY(y2, RC));
}

    DrawArrow(RC, turnX, y2, x2, y2);
}
*/
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//void __fastcall TfrmLMFS::DrawDepthLabel(TCanvas *C, TDepthLabel *L)
void __fastcall TfrmLMFS::DrawDepthLabel(const TRenderContext &RC, TDepthLabel *L)
{
    if (!L) return;

    TCanvas *C = RC.Canvas;
    const TNodeTheme th = GetThemeByKind(L->NodeKind);
    TRect R = ScaleRect(L->Rect, RC);
    int rr = ScaleValue(RC.BaseCornerRadius, RC, 3);

    C->Brush->Style = bsSolid;
    C->Brush->Color = th.BgColor;
    C->Pen->Color = th.EdgeColor;
    C->Pen->Width = ScaleValue(1, RC, 1);
    C->Font->Color = th.FontColor;
    C->Font->Name = L"맑은 고딕";
    C->Font->Style = TFontStyles() << fsBold;
    C->Font->Size = ScaleFont(9, RC, 7, 8, 14);

    if (th.Rounded)
        C->RoundRect(R.left, R.top, R.right, R.bottom, rr, rr);
    else
        C->Rectangle(R);

    String text = GetDepthLabelText(L);

    TRect outerR = R;
    InflateRect(&outerR, -ScaleValue(4, RC, 2), -ScaleValue(2, RC, 1));

    UINT flags = DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;

    SetTextColor(C->Handle, ColorToRGB(th.FontColor));
    SetBkMode(C->Handle, TRANSPARENT);

    TRect calcR = outerR;
    DrawTextW(C->Handle, text.c_str(), -1, &calcR, flags | DT_CALCRECT);

    int textH = calcR.bottom - calcR.top;
    int boxH  = outerR.bottom - outerR.top;
    int yOff  = (textH < boxH) ? ((boxH - textH) / 2) : 0;

    TRect drawR = outerR;
    drawR.top += yOff;
    drawR.bottom = drawR.top + textH;

    DrawTextW(C->Handle, text.c_str(), -1, &drawR, flags);
}
/*void __fastcall TfrmLMFS::DrawDepthLabel(const TRenderContext &RC, TDepthLabel *L)
{
    TCanvas *C = RC.Canvas;
	const TNodeTheme &th = GetThemeByKind(L->NodeKind);
	TRect R = ScaleRect(L->Rect, RC);
	//TRect R = Classes::Rect(L->Pos.x, L->Pos.y, L->Pos.x + 120, L->Pos.y + 40);
	int rr = ScaleValue(RC.BaseCornerRadius, RC, 3);

	//
	C->Brush->Style = bsClear;
	C->Pen->Color = th.EdgeColor;
	C->Brush->Color = th.BgColor;
	C->Font->Color = th.FontColor;
	C->Font->Name = L"나눔고딕";
	C->Font->Size = ScaleFont(9, RC, 7, 8, 14);//10;//[Event Depth Label]
	C->Font->Style = TFontStyles() << fsBold;

	if (th.Rounded)//노드 모서리 round 표시//
		C->RoundRect(R.left, R.top, R.right, R.bottom, rr, rr);
		//C->RoundRect(R.left, R.top, R.right, R.bottom, 10, 10);
	else
		C->Rectangle(R);


	//
	String text = GetDepthLabelText(L);

    TRect outerR = R;
	//InflateRect(&outerR, -4, -2);
	InflateRect(&outerR, -ScaleValue(4, RC, 2), -ScaleValue(2, RC, 1));

    UINT flags = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;

//??//    SetTextColor(C->Handle, ColorToRGB(th.FontColor));
//??//    SetBkMode(C->Handle, TRANSPARENT);

    TRect calcR = outerR;
    DrawTextW(C->Handle, text.c_str(), -1, &calcR, flags | DT_CALCRECT);

    int textH = calcR.bottom - calcR.top;
    int boxH  = outerR.bottom - outerR.top;
    int yOff  = (textH < boxH) ? ((boxH - textH) / 2) : 0;

    TRect drawR = outerR;
    drawR.top += yOff;
    drawR.bottom = drawR.top + textH;

	DrawTextW(C->Handle, text.c_str(), -1, &drawR, flags);

	//
//??//	C->Brush->Style = bsSolid;
}
*/
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//void __fastcall TfrmLMFS::DrawNode(TCanvas *C, TParcelNode *N)
void __fastcall TfrmLMFS::DrawNode(const TRenderContext &RC, TParcelNode *N)
{
    if (!N) return;

    if (N->Pnu == FMainPnu)
    {
        N->Selected = true;
        N->IsMain = true;
        N->NodeKind = nkMainParcel;
    }
    else
    {
        N->Selected = false;
        N->IsMain = false;
        N->NodeKind = nkSubParcel;
    }

    TCanvas *C = RC.Canvas;
    const TNodeTheme th = GetThemeByKind(N->NodeKind);
    TRect R = ScaleRect(N->Rect, RC);
    int rr = ScaleValue(RC.BaseCornerRadius, RC, 3);

    C->Brush->Style = bsSolid;
    C->Brush->Color = th.BgColor;
    C->Pen->Width = N->Selected ? ScaleValue(2, RC, 1) : ScaleValue(1, RC, 1);
    C->Pen->Color = th.EdgeColor;
    C->Font->Color = th.FontColor;
    C->Font->Name = L"나눔고딕";
    C->Font->Size = ScaleFont(10, RC, 7, 8, 16);
    C->Font->Style = TFontStyles() << fsBold;

    if (th.Rounded)
        C->RoundRect(R.left, R.top, R.right, R.bottom, rr, rr);
    else
        C->Rectangle(R);

    String text = N->Caption;
    if (RC.DrawAttr && !Trim(N->Attr).IsEmpty())
        text = text + L"\r\n" + N->Attr;

    TRect outerR = R;
    InflateRect(&outerR, -ScaleValue(4, RC, 2), -ScaleValue(2, RC, 1));

    UINT flags = DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;

    SetTextColor(C->Handle, ColorToRGB(th.FontColor));
    SetBkMode(C->Handle, TRANSPARENT);

    TRect calcR = outerR;
    DrawTextW(C->Handle, text.c_str(), -1, &calcR, flags | DT_CALCRECT);

    int textH = calcR.bottom - calcR.top;
    int boxH  = outerR.bottom - outerR.top;
    int yOff  = (textH < boxH) ? ((boxH - textH) / 2) : 0;

    TRect drawR = outerR;
    drawR.top += yOff;
    drawR.bottom = drawR.top + textH;

    DrawTextW(C->Handle, text.c_str(), -1, &drawR, flags);
}
/*void __fastcall TfrmLMFS::DrawNode(const TRenderContext &RC, TParcelNode *N)
{
    if (!N) return;

	if(N->Pnu == FMainPnu)
	{
		N->Selected = true;
		N->IsMain = true;
		N->NodeKind = nkMainParcel;
	}
	else
	{
		N->Selected = false;
		N->IsMain = false;
		N->NodeKind = nkSubParcel;
	}

    TCanvas *C = RC.Canvas;
	const TNodeTheme &th = GetThemeByKind(N->NodeKind);
	TRect R = ScaleRect(N->Rect, RC);
    int rr = ScaleValue(RC.BaseCornerRadius, RC, 3);

    C->Brush->Style = bsSolid;
    C->Pen->Width = N->Selected ? ScaleValue(2, RC, 1) : ScaleValue(1, RC, 1);
	C->Pen->Color = th.EdgeColor;
	C->Brush->Color = th.BgColor;
	C->Font->Color = th.FontColor;
	C->Font->Name = L"나눔고딕";
	//C->Font->Size = ScaleValue(10, RC, 8);
	C->Font->Size = ScaleFont(chkAttr->Checked ? 9 : 10, RC, 7, 8, 16);//[지번노드]속성 표시가 있을 때//
	C->Font->Style = TFontStyles() << fsBold;

	if (th.Rounded)
		C->RoundRect(R.left, R.top, R.right, R.bottom, rr, rr);
	else
        C->Rectangle(R);

	String text = N->Caption;
    if (RC.DrawAttr && !Trim(N->Attr).IsEmpty())
        text += L"\r\n" + N->Attr;

	PrepareTextStyle(C, th.FontColor);

    TRect outerR = R;
    InflateRect(&outerR, -ScaleValue(4, RC, 2), -ScaleValue(2, RC, 1));

	UINT flags = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;

	TRect calcR = outerR;
	DrawTextW(C->Handle, text.c_str(), -1, &calcR, flags | DT_CALCRECT);

	int textH = calcR.bottom - calcR.top;
	int boxH  = outerR.bottom - outerR.top;
    int yOff  = (textH < boxH) ? ((boxH - textH) / 2) : 0;

	TRect drawR = outerR;
	drawR.top += yOff;
	drawR.bottom = drawR.top + textH;

	DrawTextW(C->Handle, text.c_str(), -1, &drawR, flags);

	C->Brush->Style = bsSolid;
}
*/

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::PaintBox1Paint(TObject *Sender)
{
	// ZoomIn/ZoomOut
    RenderDiagram(PaintBox1->Canvas, FZoom, FOffsetX, FOffsetY, chkAttr->Checked);
/*
	TCanvas *C = PaintBox1->Canvas;

    C->Brush->Color = clWhite;
    C->FillRect(Classes::Rect(0, 0, PaintBox1->Width, PaintBox1->Height));

    C->Pen->Color = (TColor)0x00EFEFEF;
    for (int x = 0; x < PaintBox1->Width; x += 25)
    {
        C->MoveTo(x, 0);
        C->LineTo(x, PaintBox1->Height);
    }
    for (int y = 0; y < PaintBox1->Height; y += 25)
    {
        C->MoveTo(0, y);
        C->LineTo(PaintBox1->Width, y);
    }

    for (int i = 0; i < FLinks->Count; i++)
		DrawLink(C, (TDiagramLink*)FLinks->Items[i]);

    for (int i = 0; i < FLabels->Count; i++)
	{
		//C->Font->Color = clBlue;
		DrawDepthLabel(C, (TDepthLabel*)FLabels->Items[i]);
	}

	for (int i = 0; i < FNodes->Count; i++)
	{
		//C->Font->Color = clBlack;
		DrawNode(C, (TParcelNode*)FNodes->Items[i]);
	}
*/
}

//---------------------------------------------------------------------------
// ZoomIn/ZoomOut
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::RenderDiagram(TCanvas *C, double AZoom,
    int AOffsetX, int AOffsetY, bool ADrawAttr)
{
    TRenderContext RC;
    RC.Canvas = C;
    RC.Zoom = AZoom;
    RC.OffsetX = AOffsetX;
    RC.OffsetY = AOffsetY;
    RC.BaseCornerRadius = 10;
    RC.DrawAttr = ADrawAttr;

    C->Brush->Color = clWhite;
    C->FillRect(Rect(0, 0, PaintBox1->Width, PaintBox1->Height));

    C->Pen->Color = TColor(0x00EFEFEF);
    C->Pen->Width = 1;
    C->Pen->Style = psSolid;

    const int GridStep = 25;
    for (int x = 0; x < PaintBox1->Width; x += GridStep)
    {
        C->MoveTo(x, 0);
        C->LineTo(x, PaintBox1->Height);
    }

    for (int y = 0; y < PaintBox1->Height; y += GridStep)
    {
        C->MoveTo(0, y);
        C->LineTo(PaintBox1->Width, y);
    }

    for (int i = 0; i < FLinks->Count; i++)
        DrawLink(RC, (TDiagramLink*)FLinks->Items[i]);

    for (int i = 0; i < FLabels->Count; i++)
        DrawDepthLabel(RC, (TDepthLabel*)FLabels->Items[i]);

    for (int i = 0; i < FNodes->Count; i++)
        DrawNode(RC, (TParcelNode*)FNodes->Items[i]);
}

//---------------------------------------------------------------------------
// ZoomIn/ZoomOut
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::SX(int X, const TRenderContext &RC) const
{
	return RC.OffsetX + int(IRound(X * RC.Zoom));
}

//---------------------------------------------------------------------------
// ZoomIn/ZoomOut
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::SY(int Y, const TRenderContext &RC) const
{
	return RC.OffsetY + int(IRound(Y * RC.Zoom));
}

//---------------------------------------------------------------------------
// ZoomIn/ZoomOut
//---------------------------------------------------------------------------
TRect __fastcall TfrmLMFS::ScaleRect(const TRect &R, const TRenderContext &RC) const
{
    return Rect(
        SX(R.left, RC),
        SY(R.top, RC),
        SX(R.right, RC),
        SY(R.bottom, RC)
    );
}

//---------------------------------------------------------------------------
// ZoomIn/ZoomOut
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::ScaleValue(int V, const TRenderContext &RC, int AMin) const
{
    int n = int(IRound(V * RC.Zoom));
    return (n < AMin) ? AMin : n;
}

//---------------------------------------------------------------------------
// ZoomIn/ZoomOut
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::ScaleFont(int baseSize, const TRenderContext &RC,
	int minSizeScreen, int minSizePrint, int maxSize) const
{
/*
	int fontSize = 10;
	if (RC.Zoom <= 0.60) fontSize = 7;
	else if (RC.Zoom <= 0.85) fontSize = 8;
	else if (RC.Zoom <= 1.20) fontSize = 10;
	else if (RC.Zoom <= 1.80) fontSize = 11;
	else if (RC.Zoom <= 2.50) fontSize = 12;
	else fontSize = 13;
*/

/*	double z = RC.Zoom;

    // 완전 선형보다 약간 완만하게
    double f = sqrt(z);

    int n = int(IRound(baseSize * f));
    if (n < minSize) n = minSize;
    if (n > maxSize) n = maxSize;
    return n;
*/
    int minSize = minSizeScreen;
    if (RC.Canvas == Printer()->Canvas)
		minSize = minSizePrint;
    //
	int n = int(IRound(baseSize * sqrt(RC.Zoom)));
    if (n < minSize) n = minSize;
    if (n > maxSize) n = maxSize;
	return n;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::IRound(double x) const
{
	return (x >= 0.0) ? (int)floor(x + 0.5) : (int)ceil(x - 0.5);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::PrepareTextStyle(TCanvas *C, TColor ATextColor)
{
    C->Font->Color = ATextColor;
    SetTextColor(C->Handle, ColorToRGB(ATextColor));
    SetBkMode(C->Handle, TRANSPARENT);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
int __fastcall TfrmLMFS::HitTestNode(int X, int Y)
{
    for (int i = FNodes->Count - 1; i >= 0; i--)
    {
        TParcelNode *N = (TParcelNode*)FNodes->Items[i];
        if (PtInRect(&N->Rect, Point(X, Y)))
            return i;
    }
    return -1;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::PaintBox1MouseDown(TObject *Sender, TMouseButton Button,
    TShiftState Shift, int X, int Y)
{
	if (Button != mbLeft) return;

	for (int i = 0; i < FNodes->Count; i++)
        ((TParcelNode*)FNodes->Items[i])->Selected = false;

    int idx = HitTestNode(X, Y);
    if (idx >= 0)
    {
        TParcelNode *N = (TParcelNode*)FNodes->Items[idx];
        N->Selected = true;
        FDragging = true;
		FDragNodeIndex = idx;
		FDragOffset = Point(X - N->Rect.left, Y - N->Rect.top);

		//---------------------------------------------------------
		// LandArchive로 검색지번 보내기
		//---------------------------------------------------------
		if(Application->MessageBox(L"지적문서통합관리시스템(랜드아카이브)에서 해당 지번으로 검색할까요?\n", L"알림", MB_YESNO) == IDYES)
		{
			SendPnuToLandArchive(N->Pnu, N->Caption);
		}

		//---------------------------------------------------------
		//다른 노드 선택했으므로 MainPnu, MainJibun 변경//
		//---------------------------------------------------------
		FMainPnu = N->Pnu;
		FMainJibun = N->Caption;
	}

	PaintBox1->Invalidate();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::PaintBox1MouseMove(TObject *Sender, TShiftState Shift,
    int X, int Y)
{
    if (!FDragging || FDragNodeIndex < 0) return;

    TParcelNode *N = (TParcelNode*)FNodes->Items[FDragNodeIndex];
    int w = N->Rect.right - N->Rect.left;
    int h = N->Rect.bottom - N->Rect.top;

    N->Rect.left = X - FDragOffset.x;
    N->Rect.top = Y - FDragOffset.y;
    N->Rect.right = N->Rect.left + w;
    N->Rect.bottom = N->Rect.top + h;

    PaintBox1->Invalidate();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::PaintBox1MouseUp(TObject *Sender, TMouseButton Button,
    TShiftState Shift, int X, int Y)
{
    FDragging = false;
    FDragNodeIndex = -1;

    ResolveJumpFlags();
    PaintBox1->Invalidate();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool __fastcall TfrmLMFS::LoadSidoCodeCsv(const String &AFileName)
{
    if (!FileExists(AFileName))
        return false;

    TStringList *lines = new TStringList();
    TStringList *cols = new TStringList();

    try
    {
        lines->LoadFromFile(AFileName);

        cols->Delimiter = ',';
        cols->StrictDelimiter = true;

        FSidoCodeMap->Clear();

        for (int i = 1; i < lines->Count; i++)
        {
            String line = Trim(lines->Strings[i]);
            if (line.IsEmpty()) continue;

            cols->DelimitedText = line;
            if (cols->Count < 7) continue;

            String sidosgg = Trim(cols->Strings[0]); // 44770
            String umd     = Trim(cols->Strings[1]); // 250
            String ri      = Trim(cols->Strings[2]); // 27
            String umdNm   = Trim(cols->Strings[5]); // 장항읍
            String riNm    = Trim(cols->Strings[6]); // 송림리

            String code10 = sidosgg + umd + ri;
            String areaNm = umdNm;
            if (!riNm.IsEmpty())
                areaNm = umdNm + L" " + riNm;

            FSidoCodeMap->Values[code10] = areaNm;
        }
    }
    __finally
    {
        delete cols;
        delete lines;
    }

    return FSidoCodeMap->Count > 0;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::GetAreaNameByCode10(const String &ACode10)
{
    if (ACode10 == FLastAreaCode10)
        return FLastAreaName;

    int idx = FSidoCodeMap->IndexOfName(ACode10);
    String result = L"";
    if (idx >= 0)
        result = FSidoCodeMap->ValueFromIndex[idx];

    FLastAreaCode10 = ACode10;
    FLastAreaName = result;

    return result;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::MakeJibunText(const String &APnu)
{
    if (APnu.Length() < 19)
        return APnu;

    String code10 = APnu.SubString(1, 10);
    String sanFlag = APnu.SubString(11, 1);
    String bonbun = APnu.SubString(12, 4);
    String bubun  = APnu.SubString(16, 4);

    int bon = StrToIntDef(bonbun, 0);
    int bu  = StrToIntDef(bubun, 0);

    String areaName = GetAreaNameByCode10(code10);
    String jibun = (sanFlag == L"2") ? L"산 " : L"";
    jibun += IntToStr(bon);
    if (bu > 0)
        jibun += L"-" + IntToStr(bu);

    if (!areaName.IsEmpty())
        return areaName + L" " + jibun;

    return jibun;
}

//---------------------------------------------------------------------------
// PNU별 1회만 변환되도록 캐시를 보는 함수//
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::GetOrCreateJibun(const String &APnu)
{
    String val;

	int idx = FPnuCaptionCache->IndexOfName(APnu);
    if (idx >= 0)
        return FPnuCaptionCache->ValueFromIndex[idx];

    val = MakeJibunText(APnu);
    FPnuCaptionCache->Values[APnu] = val;
    return val;
}

//---------------------------------------------------------------------------
// 지목코드 데이터 읽기//
//---------------------------------------------------------------------------
bool __fastcall TfrmLMFS::LoadJimokCsv(const String &AFileName)
{
    FJimokMap->Clear();

    if (!FileExists(AFileName))
        return false;

    TStringList *lines = new TStringList();
    TStringList *cols  = new TStringList();

    try
    {
        lines->LoadFromFile(AFileName);

        cols->StrictDelimiter = true;
        cols->Delimiter = ',';

        for (int i = 0; i < lines->Count; i++)
        {
            String line = Trim(lines->Strings[i]);
            if (line.IsEmpty()) continue;
            if (i == 0) continue; // header skip

            cols->DelimitedText = line;
            if (cols->Count < 2) continue;

            String code = Trim(cols->Strings[0]);
            String name = Trim(cols->Strings[1]);

            if (!code.IsEmpty() && !name.IsEmpty())
                FJimokMap->Add(code + L"=" + name);
        }
    }
    __finally
    {
        delete cols;
        delete lines;
    }

	return FJimokMap->Count > 0;
}

//---------------------------------------------------------------------------
// 지목코드 => 지목명 매칭하기
//---------------------------------------------------------------------------
String __fastcall TfrmLMFS::GetJimokName(const String &ACode)
{
    String code = Trim(ACode);
    if (code.IsEmpty()) return L"";

    int idx = FJimokMap->IndexOfName(code);
    if (idx >= 0)
        return FJimokMap->ValueFromIndex[idx];

    return code; // 못 찾으면 원래 코드 반환
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::SaveDiagramToPng(const String &AFileName)
{
    Graphics::TBitmap *bmp = new Graphics::TBitmap();
    TPngImage *png = new TPngImage();
    try
    {
		TRenderContext RC;
		RC.Canvas = bmp->Canvas;
        RC.Zoom = FZoom;
        RC.OffsetX = FOffsetX;
		RC.OffsetY = FOffsetY;
		RC.BaseCornerRadius = 10;
		RC.DrawAttr = chkAttr->Checked;

		bmp->PixelFormat = pf24bit;
        bmp->Width = PaintBox1->Width;
		bmp->Height = PaintBox1->Height;
        bmp->Canvas->Brush->Color = clWhite;
        bmp->Canvas->FillRect(Rect(0, 0, bmp->Width, bmp->Height));

        for (int i = 0; i < FLinks->Count; i++)
			DrawLink(RC, (TDiagramLink*)FLinks->Items[i]);

        for (int i = 0; i < FLabels->Count; i++)
			DrawDepthLabel(RC, (TDepthLabel*)FLabels->Items[i]);

        for (int i = 0; i < FNodes->Count; i++)
			DrawNode(RC, (TParcelNode*)FNodes->Items[i]);

        png->Assign(bmp);
        png->SaveToFile(AFileName);
    }
    __finally
	{
		Application->MessageBox(L"다이어그램을 저장하였습니다.\n", L"알림", MB_OK);

        delete png;
        delete bmp;
    }
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::chkMainClick(TObject *Sender)
{
	AnalyzeRowsToDiagram(FRows);
	RefreshDiagram();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::chkAttrClick(TObject *Sender)
{
	//다시 그리기//
	BuildLayout();
	ResolveJumpFlags();
	PaintBox1->Invalidate();
}

//---------------------------------------------------------------------------
// BF_PNU, AF_PNU 공통 추가 함수
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::AddJibunToGridMap(TStringList *AMap, const String &APnu, const String &AJibun)
{
	String pnu = Trim(APnu);
	String jibun = Trim(AJibun);

	if (pnu.IsEmpty() || jibun.IsEmpty()) return;
    if (pnu.Length() < 19) return;

    // "지번명=대표PNU"
	// 지번명이 같으면 첫 번째 것만 유지
	if (AMap->IndexOfName(jibun) < 0)
		//Sorted=True이면 Add()가 알아서 적절한 위치에 넣습니다.
		AMap->Add(jibun + L"=" + pnu);//Error//AMap->Values[jibun] = pnu;
}

//---------------------------------------------------------------------------
// 지번목록 Grid 초기화(구조)//
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::CreateGrid()
{
	StringGrid1->RowCount  = 2;
	StringGrid1->ColCount  = 2;
	StringGrid1->FixedRows = 1;
	StringGrid1->ColWidths[0] = StringGrid1->DefaultColWidth;//Jibun
	StringGrid1->ColWidths[1] = 0;//Pnu//Hide//

	StringGrid1->Cells[0][0] = L"지번";
	StringGrid1->Cells[1][0] = L"PNU";

	::UpdateWindow(StringGrid1->Handle);
}

//---------------------------------------------------------------------------
// 지번목록 Grid 초기화(데이터)//
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::InitGrid()
{
	for(int i = 1; i < StringGrid1->RowCount; i++)
		for(int j = 1; j < StringGrid1->ColCount; j++)
			StringGrid1->Cells[j][i]	= "";
}

//---------------------------------------------------------------------------
// ARows 전체에서 Grid 목록 생성
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::DisplayGrid(const std::vector<TFlowRow> &ARows)
{
	//---------------------------------------------------------------------------
	// 지번목록 Grid 초기화(데이터)//
	//---------------------------------------------------------------------------
	InitGrid();

	//---------------------------------------------------------------------------
	// 데이터 구성하기//
	//---------------------------------------------------------------------------
	TStringList *gridMap = new TStringList();
    try
	{
		gridMap->NameValueSeparator = '=';
		gridMap->Sorted = true;
		gridMap->Duplicates = dupIgnore;

		for (size_t i = 0; i < ARows.size(); i++)
		{
			AddJibunToGridMap(gridMap, ARows[i].BfPnu, ARows[i].BfJibun);
			AddJibunToGridMap(gridMap, ARows[i].AfPnu, ARows[i].AfJibun);
		}

		StringGrid1->RowCount = gridMap->Count + 1;

		for (int i = 0; i < gridMap->Count; i++)
		{
			String jibun = gridMap->Names[i];
			String pnu   = gridMap->ValueFromIndex[i];

			StringGrid1->Cells[0][i + 1] = jibun;
			StringGrid1->Cells[1][i + 1] = pnu;
		}
	}
	__finally
    {
        delete gridMap;
	}

	if(StringGrid1->RowCount <= 25+1)
	{
		this->Width = 1580;
		pnlTool->Width = 160;
	}
	else
	{
		this->Width = 1580 + 17;
		pnlTool->Width = 160 + 17;
	}
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::StringGrid1SelectCell(TObject *Sender, int ACol, int ARow,
          bool &CanSelect)
{
    // 행이 바뀔 때마다 그리드 전체를 다시 그리도록 강제 명령 (배경색 즉시 반영)
	StringGrid1->Invalidate();

	if (ARow <= 0) return;

	FMainPnuMap->Clear();//[대표이사REQ]

	String pnu = Trim(StringGrid1->Cells[1][ARow]);
	String jibun = Trim(StringGrid1->Cells[0][ARow]);
	if (pnu.IsEmpty() || jibun.IsEmpty()) return;
	if (pnu == FMainPnu) return;

	//메인 PNU(지번) 변경
	FMainPnu = pnu;
	FMainJibun = jibun;
	if( chkMain->Checked )
		AnalyzeRowsToDiagram(FRows);
	RefreshDiagram();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
/*void __fastcall TfrmLMFS::btnSavePngClick(TObject *Sender)
{
	AnsiString  asFileName = DOWNLOADPATH + FMainJibun + ".png";
	//
	SaveDiagramToPng(asFileName);

	String  sMsg = "";
	if(FileExists(asFileName))
	{
		sMsg.sprintf(L"이미지파일로 저장하였습니다. DOWNLOAD 폴더를 확인해 주세요.\n");
		ShellExecute(NULL, "open", DOWNLOADPATH.c_str(), NULL, NULL, SW_SHOWNORMAL);
	}
	else
	{
		sMsg.sprintf(L"저장하지 못했습니다.\n");
	}

	Application->MessageBox(sMsg.c_str(), L"알림", MB_OK);
}
*/
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::btnSaveClick(TObject *Sender)
{
/*	String pdfFile = ExtractFilePath(Application->ExeName) +
					 L"DOWNLOAD\\" + FMainJibun + L".pdf";

	ForceDirectories(ExtractFilePath(pdfFile));

	if (PrintToPdfDirect(pdfFile))
	{
		Application->MessageBox(L"PDF 저장이 완료되었습니다.",
								L"PDF 출력", MB_OK | MB_ICONINFORMATION);
	}
*/

	ExportToPdfByPrinter();
}

//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::ExportToPdfByPrinter()
{
    int pdfIdx = Printer()->Printers->IndexOf(L"Microsoft Print to PDF");
    if (pdfIdx < 0)
    {
        ShowMessage(L"Microsoft Print to PDF 프린터를 찾을 수 없습니다.");
        return;
    }

    Printer()->PrinterIndex = pdfIdx;
    Printer()->Orientation = poLandscape;
	//Printer()->Title = ((FMainJibun == NULL) ? L"Diagram Export" : FMainJibun);
	if(FMainJibun == "")
		Printer()->Title = L"Diagram Export";
	else
		Printer()->Title = FMainJibun;

    const int margin = 120;

    Printer()->BeginDoc();
    try
    {
        int pageW = Printer()->PageWidth  - margin * 2;
        int pageH = Printer()->PageHeight - margin * 2;

        if (pageW <= 0 || pageH <= 0)
            throw Exception(L"PDF 출력 가능 영역이 너무 작습니다.");

        double zoomY = (double)pageH / (double)FDiagramHeight;
        double printZoom = zoomY;

        if (printZoom > 0.80) printZoom = 0.80;
        if (printZoom < 0.70) printZoom = 0.70;

        int logicalPageW = (int)(pageW / printZoom);
        if (logicalPageW <= 0)
            throw Exception(L"페이지 폭 계산 오류");

        int pageCount = (FDiagramWidth + logicalPageW - 1) / logicalPageW;
        if (pageCount < 1) pageCount = 1;

        for (int page = 0; page < pageCount; ++page)
        {
            if (page > 0)
                Printer()->NewPage();

            Printer()->Canvas->Brush->Color = clWhite;
            Printer()->Canvas->FillRect(Rect(0, 0, Printer()->PageWidth, Printer()->PageHeight));

            int logicalOffsetX = page * logicalPageW;

            RenderDiagram(
                Printer()->Canvas,
                printZoom,
                margin - (int)IRound(logicalOffsetX * printZoom),
                margin,
                chkAttr->Checked
            );
        }
    }
    __finally
    {
        Printer()->EndDoc();
    }
}
/*bool __fastcall TfrmLMFS::PrintToPdfDirect(const String &APdfFileName)
{
    int oldPrinterIndex = Printer()->PrinterIndex;
    int pdfIndex = Printer()->Printers->IndexOf(L"Microsoft Print to PDF");

    if (pdfIndex < 0)
    {
        Application->MessageBox(L"'Microsoft Print to PDF' 프린터를 찾을 수 없습니다.",
                                L"PDF 출력", MB_OK | MB_ICONERROR);
        return false;
    }

    wchar_t DeviceName[256] = {0};
    wchar_t DriverName[256] = {0};
	wchar_t PortName[256]   = {0};
    THandle DeviceMode = 0;

    try
    {
        Printer()->PrinterIndex = pdfIndex;
        Printer()->GetPrinter(DeviceName, DriverName, PortName, DeviceMode);

        Printer()->SetPrinter(DeviceName, DriverName, APdfFileName.c_str(), 0);
        Printer()->Title = L"LMFS Diagram";

        Printer()->BeginDoc();
        try
		{
            // 여기서 실제 출력
            RenderDiagram(Printer()->Canvas, 1.0, 40, 40, chkAttr->Checked);

            // 여러 페이지가 필요하면:
            // Printer()->NewPage();
            // RenderDiagram(...);
        }
        __finally
        {
            Printer()->EndDoc();
        }
    }
    __finally
    {
        Printer()->PrinterIndex = oldPrinterIndex;
    }

    return true;
}
*/
//---------------------------------------------------------------------------
// Send SearchPnu To LandArchive : 검색지번 보내기//
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::SendPnuToLandArchive(const String &APnu, const String &AJibun)
{
	HWND hZ = FindWindow(NULL, "지적문서통합관리시스템(랜드아카이브)"); // 또는 클래스명 지정
	if (!hZ)
	{
		Application->MessageBox(L"지적문서통합관리시스템(랜드아카이브) 프로그램을 찾지 못했습니다.\n", L"알림", MB_OK);
		return;
	}

	String msg = L"PNU=" + APnu + L"\nJIBUN=" + AJibun;

	COPYDATASTRUCT cds;
	cds.dwData = 1001;
	cds.cbData = (msg.Length() + 1) * sizeof(wchar_t);
	cds.lpData = (void*)msg.c_str();

	SendMessage(hZ, WM_COPYDATA, (WPARAM)Handle, (LPARAM)&cds);
}

//---------------------------------------------------------------------------
// Receive MainPnu From LandArchive : 검색지번 받고 다이어그램 다시 그리기//
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::WMCopyData(TWMCopyData &Msg)
{
    if (!Msg.CopyDataStruct) return;

    if (Msg.CopyDataStruct->dwData == 1002)
    {
        String s = (wchar_t*)Msg.CopyDataStruct->lpData;

		String pnu = L"";
		String jibun = L"";
        TStringList *lines = new TStringList();
        try
        {
            lines->Text = s;
            for (int i = 0; i < lines->Count; i++)
            {
                String line = Trim(lines->Strings[i]);
				if (line.Pos(L"PNU=") == 1)
					pnu = line.SubString(5, line.Length() - 4);
                else if (line.Pos(L"JIBUN=") == 1)
					jibun = line.SubString(7, line.Length() - 6);
			}
		}
		__finally
		{
			delete lines;
		}

		if (!pnu.IsEmpty())
		{
			FMainPnu = pnu;
			FMainJibun = jibun;
			//기존 데이터에 속하는 지번인지 알수 없어 PNU.csv 파일부터 읽기//
			LoadDiagramData();//RefreshDiagram();
		}
    }

    Msg.Result = 1;
}

//---------------------------------------------------------------------------
// YYYYMMDD => YYYY-MM-DD
//---------------------------------------------------------------------------
AnsiString __fastcall TfrmLMFS::funcChangeDateFormatString(AnsiString _asDateStr)
{
	//YYYY-MM-DD
	if(_asDateStr.Length() == 8)
		_asDateStr = _asDateStr.SubString(1,4) + "-" + _asDateStr.SubString(5,2) + "-" + _asDateStr.SubString(7,2);

	return _asDateStr;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::StringGrid1DrawCell(TObject *Sender, int ACol, int ARow, TRect &Rect, TGridDrawState State)
{
    // 1. 헤더 영역 (Row 0) 처리 - 기존과 동일하게 가운데 정렬 유지
    if (ARow == 0)
    {
        StringGrid1->Canvas->Brush->Color = StringGrid1->FixedColor;
        StringGrid1->Canvas->Font->Color = StringGrid1->Font->Color; // 기본 폰트 색상
        StringGrid1->Canvas->FillRect(Rect);

        DrawTextW(StringGrid1->Canvas->Handle,
                 StringGrid1->Cells[ACol][ARow].c_str(),
                 -1,
                 &Rect,
                 DT_SINGLELINE | DT_CENTER | DT_VCENTER);
    }
    // 2. 일반 데이터 영역 처리
    else
    {
        // 핵심: 현재 그리고 있는 행(ARow)이 사용자가 선택한 행(StringGrid1->Row)과 일치하는가?
        if (ARow == StringGrid1->Row)
        {
            // 원하시는 강조 색상으로 커스텀 지정 (예: 노란색 배경에 검은 글씨)
            StringGrid1->Canvas->Brush->Color = clYellow;      // 선택된 행 배경색
            StringGrid1->Canvas->Font->Color  = clBlack;       // 선택된 행 글자색

            // 만약 윈도우 기본 하이라이트(파란색)를 쓰고 싶다면 아래 주석을 해제하세요.
            // StringGrid1->Canvas->Brush->Color = clHighlight;
            // StringGrid1->Canvas->Font->Color  = clHighlightText;
        }
        else
        {
            // 선택되지 않은 나머지 일반 행의 색상
            StringGrid1->Canvas->Brush->Color = StringGrid1->Color;
            StringGrid1->Canvas->Font->Color  = StringGrid1->Font->Color;
        }

        // 배경 채우기 및 텍스트 그리기
        StringGrid1->Canvas->FillRect(Rect);

        TRect textRect = Rect;
        textRect.Left += 4; // 왼쪽 여백 4픽셀

        DrawTextW(StringGrid1->Canvas->Handle,
                 StringGrid1->Cells[ACol][ARow].c_str(),
                 -1,
                 &textRect,
                 DT_SINGLELINE | DT_LEFT | DT_VCENTER);
    }

    // 3. 포커스된 셀의 테두리 그리기 (필요 없으면 이 블록을 삭제하여 테두리를 깔끔하게 지울 수 있습니다)
    if (State.Contains(gdFocused))
    {
        StringGrid1->Canvas->DrawFocusRect(Rect);
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::SetDefaultThemes()
{
	oldMain.BgColor   = (TColor)RGB(237, 125, 49);
	oldMain.EdgeColor = (TColor)RGB(200, 200, 200);//clSilver
	oldMain.FontColor = clWhite;
	oldMain.Rounded   = true;

	oldSub.BgColor   = (TColor)RGB(165, 165, 165);
	oldSub.EdgeColor = (TColor)RGB(200, 200, 200);//clSilver
	oldSub.FontColor = clWhite;
	oldSub.Rounded   = false;

	oldEvent.BgColor   = clWhite;
	oldEvent.EdgeColor = (TColor)RGB(0, 128, 0);
	oldEvent.FontColor = (TColor)RGB(0, 128, 0);
	oldEvent.Rounded   = false;


	oldMain.BgColor   = (TColor)RGB(237, 125, 49);
	oldMain.EdgeColor = (TColor)RGB(200, 200, 200);//clSilver
	oldMain.FontColor = clWhite;
	oldMain.Rounded   = true;

	oldSub.BgColor   = (TColor)RGB(165, 165, 165);
	oldSub.EdgeColor = (TColor)RGB(200, 200, 200);//clSilver
	oldSub.FontColor = clWhite;
	oldSub.Rounded   = false;

	oldEvent.BgColor   = clWhite;
	oldEvent.EdgeColor = (TColor)RGB(0, 128, 0);
	oldEvent.FontColor = (TColor)RGB(0, 128, 0);
	oldEvent.Rounded   = false;

	pnlColorMainBg->ParentBackground	= false;
	pnlColorMainEdge->ParentBackground 	= false;
	pnlColorMainFont->ParentBackground 	= false;
	pnlColorSubBg->ParentBackground 	= false;
	pnlColorSubEdge->ParentBackground 	= false;
	pnlColorSubFont->ParentBackground 	= false;
	pnlColorEventBg->ParentBackground 	= false;
	pnlColorEventEdge->ParentBackground = false;
	pnlColorEventFont->ParentBackground = false;

/*
	btnColorMainBg->Color 	= FThemeMainNode.BgColor;
	btnColorMainEdge->Color = FThemeMainNode.EdgeColor;
	btnColorMainFont->Color = FThemeMainNode.FontColor;
	btnColorSubBg->Color 	= FThemeSubNode.BgColor;
	btnColorSubEdge->Color 	= FThemeSubNode.EdgeColor;
	btnColorSubFont->Color 	= FThemeSubNode.FontColor;
	btnColorEventBg->Color 	= FThemeEventNode.BgColor;
	btnColorEventEdge->Color = FThemeEventNode.EdgeColor;
	btnColorEventFont->Color = FThemeEventNode.FontColor;
*/
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::LoadThemesFromIni()
{
	//미설정_기본값 가져오기//
	SetDefaultThemes();

	//사용자 설정값 가져오기//
	String iniFile = ExtractFilePath(Application->ExeName) + L"ConfigLMFS.ini";
	TIniFile *ini = new TIniFile(iniFile);
	try
	{
		FThemeMainNode.BgColor   = (TColor)ini->ReadInteger(L"MainNode",  L"BackColor",   oldMain.BgColor);
		FThemeMainNode.EdgeColor = (TColor)ini->ReadInteger(L"MainNode",  L"BorderColor", oldMain.EdgeColor);//clSilver
		FThemeMainNode.FontColor = (TColor)ini->ReadInteger(L"MainNode",  L"TextColor",   oldMain.FontColor);//clWhite
		FThemeMainNode.Rounded   = ini->ReadBool   (L"MainNode",  L"Rounded",    0);

		FThemeSubNode.BgColor   = (TColor)ini->ReadInteger(L"SubNode",   L"BackColor",   oldSub.BgColor);
		FThemeSubNode.EdgeColor = (TColor)ini->ReadInteger(L"SubNode",   L"BorderColor", oldSub.EdgeColor);//clSilver
		FThemeSubNode.FontColor = (TColor)ini->ReadInteger(L"SubNode",   L"TextColor",   oldSub.FontColor);//clWhite
		FThemeSubNode.Rounded   = ini->ReadBool   (L"SubNode",   L"Rounded",    0);

		FThemeEventNode.BgColor   = (TColor)ini->ReadInteger(L"EventNode", L"BackColor",   oldEvent.BgColor);//clWhite
		FThemeEventNode.EdgeColor = (TColor)ini->ReadInteger(L"EventNode", L"BorderColor", oldEvent.EdgeColor);//clGreen
		FThemeEventNode.FontColor = (TColor)ini->ReadInteger(L"EventNode", L"TextColor",   oldEvent.FontColor);//clGreen
		FThemeEventNode.Rounded   = ini->ReadBool   (L"EventNode", L"Rounded",    0);
    }
    __finally
    {
		delete ini;
	}

	pnlColorMainBg->Color 	= FThemeMainNode.BgColor;
	pnlColorMainEdge->Color = FThemeMainNode.EdgeColor;
	pnlColorMainFont->Color = FThemeMainNode.FontColor;
	pnlColorSubBg->Color 	= FThemeSubNode.BgColor;
	pnlColorSubEdge->Color 	= FThemeSubNode.EdgeColor;
	pnlColorSubFont->Color 	= FThemeSubNode.FontColor;
	pnlColorEventBg->Color 	= FThemeEventNode.BgColor;
	pnlColorEventEdge->Color = FThemeEventNode.EdgeColor;
	pnlColorEventFont->Color = FThemeEventNode.FontColor;

	//사용자 설정값 백업하기//
	oldMain 	= FThemeMainNode;
	oldSub  	= FThemeSubNode;
	oldEvent 	= FThemeEventNode;
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::SaveThemesToIni()
{
	String iniFile = ExtractFilePath(Application->ExeName) + L"configLMFS.ini";
    TIniFile *ini = new TIniFile(iniFile);
    try
	{
		ini->WriteInteger(L"MainNode",  L"BackColor",   (int)FThemeMainNode.BgColor);
		ini->WriteInteger(L"MainNode",  L"BorderColor", (int)FThemeMainNode.EdgeColor);
		ini->WriteInteger(L"MainNode",  L"TextColor",   (int)FThemeMainNode.FontColor);
        ini->WriteBool   (L"MainNode",  L"Rounded",     FThemeMainNode.Rounded);

		ini->WriteInteger(L"SubNode",   L"BackColor",   (int)FThemeSubNode.BgColor);
		ini->WriteInteger(L"SubNode",   L"BorderColor", (int)FThemeSubNode.EdgeColor);
		ini->WriteInteger(L"SubNode",   L"TextColor",   (int)FThemeSubNode.FontColor);
        ini->WriteBool   (L"SubNode",   L"Rounded",     FThemeSubNode.Rounded);

		ini->WriteInteger(L"EventNode", L"BackColor",   (int)FThemeEventNode.BgColor);
		ini->WriteInteger(L"EventNode", L"BorderColor", (int)FThemeEventNode.EdgeColor);
		ini->WriteInteger(L"EventNode", L"TextColor",   (int)FThemeEventNode.FontColor);
        ini->WriteBool   (L"EventNode", L"Rounded",     FThemeEventNode.Rounded);
    }
    __finally
    {
        delete ini;
    }
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::gbColorClick(TObject *Sender)
{
	if(Sender == pnlColorMainBg)
		ColorDialog1->Color = FThemeMainNode.BgColor;
	else if(Sender == pnlColorMainEdge)
		ColorDialog1->Color = FThemeMainNode.EdgeColor;
	else if(Sender == pnlColorMainFont)
		ColorDialog1->Color = FThemeMainNode.FontColor;
	else if(Sender == pnlColorSubBg)
		ColorDialog1->Color = FThemeSubNode.BgColor;
	else if(Sender == pnlColorSubEdge)
		ColorDialog1->Color = FThemeSubNode.EdgeColor;
	else if(Sender == pnlColorSubFont)
		ColorDialog1->Color = FThemeSubNode.FontColor;
	else if(Sender == pnlColorEventBg)
		ColorDialog1->Color = FThemeEventNode.BgColor;
	else if(Sender == pnlColorEventEdge)
		ColorDialog1->Color = FThemeEventNode.EdgeColor;
	else if(Sender == pnlColorEventFont)
		ColorDialog1->Color = FThemeEventNode.FontColor;

	if (ColorDialog1->Execute())
	{
		if(Sender == btnColorMainBg)
		{
			FThemeMainNode.BgColor = ColorDialog1->Color;
			pnlColorMainBg->Color = FThemeMainNode.BgColor;
		}
		else if(Sender == btnColorMainEdge)
		{
			FThemeMainNode.EdgeColor = ColorDialog1->Color;
			pnlColorMainEdge->Color = FThemeMainNode.EdgeColor;
		}
		else if(Sender == btnColorMainFont)
		{
			FThemeMainNode.FontColor = ColorDialog1->Color;
			pnlColorMainFont->Color = FThemeMainNode.FontColor;
		}
		else if(Sender == btnColorSubBg)
		{
			FThemeSubNode.BgColor = ColorDialog1->Color;
			pnlColorSubBg->Color = FThemeSubNode.BgColor;
		}
		else if(Sender == btnColorSubEdge)
		{
			FThemeSubNode.EdgeColor = ColorDialog1->Color;
			pnlColorSubEdge->Color = FThemeSubNode.EdgeColor;
		}
		else if(Sender == btnColorSubFont)
		{
			FThemeSubNode.FontColor = ColorDialog1->Color;
			pnlColorSubFont->Color = FThemeSubNode.FontColor;
		}
		else if(Sender == btnColorEventBg)
		{
			FThemeEventNode.BgColor = ColorDialog1->Color;
			pnlColorEventBg->Color = FThemeEventNode.BgColor;
		}
		else if(Sender == btnColorEventEdge)
		{
			FThemeEventNode.EdgeColor = ColorDialog1->Color;
			pnlColorEventEdge->Color = FThemeEventNode.EdgeColor;
		}
		else if(Sender == btnColorEventFont)
		{
			FThemeEventNode.FontColor = ColorDialog1->Color;
			pnlColorEventFont->Color = FThemeEventNode.FontColor;
		}

		ApplyToOwner();
	}
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::ApplyToOwner()
{
	PaintBox1->Repaint();
	//PreviewPaintBox->Repaint();
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::btnDiagramSettingClick(TObject *Sender)
{
	if( !pnlDiagramSetting->Visible )
	{
//		LoadThemesFromIni();
//		PaintBox1->Repaint();
	}
	else
	{
		SaveThemesToIni();
		PaintBox1->Repaint();
	}

	pnlDiagramSetting->Visible = !pnlDiagramSetting->Visible;
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::btnDiagramSettingCloseClick(TObject *Sender)
{
	pnlDiagramSetting->Visible = false;
}
//---------------------------------------------------------------------------

const TNodeTheme& __fastcall TfrmLMFS::GetThemeByKind(TNodeKind AKind)
{
	switch (AKind)
	{
		case nkMainParcel: return FThemeMainNode;
		case nkSubParcel:  return FThemeSubNode;
		case nkEvent:      return FThemeEventNode;
		default:           return FThemeSubNode;
	}
}
//---------------------------------------------------------------------------


void __fastcall TfrmLMFS::btnSettingSaveClick(TObject *Sender)
{
//	FThemeMainNode = oldMain;
//	FThemeSubNode = oldSub;
//	FThemeEventNode = oldEvent;
	PaintBox1->Repaint();
}
//---------------------------------------------------------------------------

void __fastcall TfrmLMFS::pnlColorClick(TObject *Sender)
{
	gbColorClick(Sender);
}
//---------------------------------------------------------------------------


void __fastcall TfrmLMFS::FormMouseWheel(TObject *Sender, TShiftState Shift, int WheelDelta,
          TPoint &MousePos, bool &Handled)
{
    if (!Shift.Contains(ssCtrl))
        return;

    if (WheelDelta > 0)
		FZoom *= 1.1;
    else
        FZoom /= 1.1;

    if (FZoom < 0.25)
        FZoom = 0.25;
	if (FZoom > 2.0)//4.0
		FZoom = 2.0;//4.0

	//
	UpdateCanvasSize();
	PaintBox1->Repaint();
    Handled = true;
}
//---------------------------------------------------------------------------

