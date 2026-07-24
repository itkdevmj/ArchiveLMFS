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
			TParcelNode *fromNode = AddOrGetNode(depth, R.BfPnu, R.BfJibun, MakeAttr(R, false));
			TParcelNode *toNode   = AddOrGetNode(depth + 1, R.AfPnu, R.AfJibun, MakeAttr(R, true));

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
	const int NodeW = 140;
	const int NodeH1 = 34;
	const int NodeH2 = 56;
	const int LeftM = 40;
	const int TopM  = 40;
	const int GapX  = 280;
	const int GapY  = 20;

	bool showAttr = chkAttr->Checked;

	for (int i = 0; i < FNodes->Count; i++)
	{
		TParcelNode *N = (TParcelNode*)FNodes->Items[i];

		int nodeH = showAttr ? NodeH2 : NodeH1;
		int x = LeftM + N->Depth * GapX;
		int y = TopM + N->Lane * (nodeH + GapY);

		N->Rect = Classes::Rect(x, y, x + NodeW, y + nodeH);
	}

	for (int i = 0; i < FLabels->Count; i++)
	{
		TDepthLabel *L = (TDepthLabel*)FLabels->Items[i];
		int x = LeftM + L->Depth * GapX + NodeW + 10;
		int y = 14;
        L->Pos = Point(x, y);
    }

    UpdateCanvasSize();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::UpdateCanvasSize()
{
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
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::DrawArrow(TCanvas *C, int x1, int y1, int x2, int y2)
{
    double angle = atan2((double)(y2 - y1), (double)(x2 - x1));
    int len = 8;

    double a1 = angle + 3.1415926535 * 0.85;
    double a2 = angle - 3.1415926535 * 0.85;

    int ax1 = x2 + (int)(cos(a1) * len);
    int ay1 = y2 + (int)(sin(a1) * len);
    int ax2 = x2 + (int)(cos(a2) * len);
    int ay2 = y2 + (int)(sin(a2) * len);

    C->MoveTo(x2, y2); C->LineTo(ax1, ay1);
    C->MoveTo(x2, y2); C->LineTo(ax2, ay2);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::DrawJumpArc(TCanvas *C, int X, int Y)
{
    // 작은 반원 bump
    C->Arc(X - 8, Y - 8, X + 8, Y + 8, X - 8, Y, X + 8, Y);
}

void __fastcall TfrmLMFS::DrawLink(TCanvas *C, TDiagramLink *L)
{
    if (!L || !L->FromNode || !L->ToNode) return;

	TRect r1 = L->FromNode->Rect;
	TRect r2 = L->ToNode->Rect;

	L->IsMain = (FMainPnu == L->FromNode->Pnu || FMainPnu == L->FromNode->Pnu) ? (true):(false);

    int x1 = r1.right;
    int y1 = (r1.top + r1.bottom) / 2;
    int x2 = r2.left;
    int y2 = (r2.top + r2.bottom) / 2;
	int midX = (x1 + x2) / 2;

	if(L->IsMain)
	{
		C->Pen->Color = (TColor)RGB(237, 125, 49);//(TColor)RGB(0, 0, 255);//Link - Blue
		C->Pen->Width = 3;
	}
	else
	{
		C->Pen->Color = (TColor)RGB(0, 0, 0);//Link - Black
		C->Pen->Width = 1;
	}

    if (!L->IsJump)
    {
        C->MoveTo(x1, y1);
        C->LineTo(midX, y1);
        C->LineTo(midX, y2);
        C->LineTo(x2, y2);
    }
    else
    {
        int jumpX = midX;
        int jumpY = y2;

        C->MoveTo(x1, y1);
        C->LineTo(midX - 10, y1);
        C->LineTo(midX - 10, y2);
        C->LineTo(jumpX - 10, jumpY);
        DrawJumpArc(C, jumpX, jumpY);
        C->MoveTo(jumpX + 8, jumpY);
        C->LineTo(x2, y2);
    }

    DrawArrow(C, midX, y2, x2, y2);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::DrawDepthLabel(TCanvas *C, TDepthLabel *L)
{
	const TNodeTheme &th = GetThemeByKind(L->NodeKind);
	TRect R = Classes::Rect(L->Pos.x, L->Pos.y, L->Pos.x + 120, L->Pos.y + 40);

	C->Pen->Color = th.EdgeColor;
	C->Brush->Color = th.BgColor;
	C->Font->Color = th.FontColor;
	C->Font->Name = L"나눔고딕";
	C->Font->Size = 10;
	C->Font->Style = TFontStyles() << fsBold;

	if (th.Rounded)//노드 모서리 round 표시//
		C->RoundRect(R.left, R.top, R.right, R.bottom, 10, 10);
	else
		C->Rectangle(R);

	//
	C->Brush->Style = bsClear;

	//
	String text = GetDepthLabelText(L);

    TRect outerR = R;
    InflateRect(&outerR, -4, -2);

    UINT flags = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;

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

	//
	C->Brush->Style = bsSolid;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::DrawNode(TCanvas *C, TParcelNode *N)
{
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

	const TNodeTheme &th = GetThemeByKind(N->NodeKind);


/*
	C->Pen->Color = N->Selected ? clRed : clGray;
	if (N->IsMain)
		C->Brush->Color = (TColor)0x0060A8FF;   // 메인 PNU만 강조
	else
		C->Brush->Color = (TColor)0x00D8D8D8;   // 일반 노드
	C->Rectangle(N->Rect);
*/

	C->Brush->Style = bsClear;
	C->Pen->Width = N->Selected ? 2 : 1;
	C->Pen->Color = th.EdgeColor;
	C->Brush->Color = th.BgColor;
	C->Font->Color = th.FontColor;
	C->Font->Name = L"나눔고딕";
	C->Font->Size = 10;
	C->Font->Style = TFontStyles() << fsBold;

	if (th.Rounded)//노드 모서리 round 표시//
		C->RoundRect(N->Rect.left, N->Rect.top, N->Rect.right, N->Rect.bottom, 10, 10);
	else
		C->Rectangle(N->Rect);

    //
	String text = N->Caption;
    if (chkAttr->Checked && !Trim(N->Attr).IsEmpty())
        text += L"\r\n" + N->Attr;

	TRect outerR = N->Rect;
	InflateRect(&outerR, -4, -2);

	UINT flags = DT_CENTER | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX;

	TRect calcR = outerR;
	DrawTextW(C->Handle, text.c_str(), -1, &calcR, flags | DT_CALCRECT);

	int textH = calcR.bottom - calcR.top;
	int boxH  = outerR.bottom - outerR.top;
	int yOff  = 0;

	if (textH < boxH)
		yOff = (boxH - textH) / 2;

	TRect drawR = outerR;
	drawR.top += yOff;
	drawR.bottom = drawR.top + textH;

	DrawTextW(C->Handle, text.c_str(), -1, &drawR, flags);

	C->Brush->Style = bsSolid;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TfrmLMFS::PaintBox1Paint(TObject *Sender)
{
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
        bmp->PixelFormat = pf24bit;
        bmp->Width = PaintBox1->Width;
        bmp->Height = PaintBox1->Height;

        bmp->Canvas->Brush->Color = clWhite;
        bmp->Canvas->FillRect(Rect(0, 0, bmp->Width, bmp->Height));

        for (int i = 0; i < FLinks->Count; i++)
			DrawLink(bmp->Canvas, (TDiagramLink*)FLinks->Items[i]);

        for (int i = 0; i < FLabels->Count; i++)
            DrawDepthLabel(bmp->Canvas, (TDepthLabel*)FLabels->Items[i]);

        for (int i = 0; i < FNodes->Count; i++)
			DrawNode(bmp->Canvas, (TParcelNode*)FNodes->Items[i]);

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
void __fastcall TfrmLMFS::btnSavePngClick(TObject *Sender)
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
//---------------------------------------------------------------------------

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


/* 폰트 변경
C->Font->Name = L"맑은 고딕";
C->Font->Size = 9;
C->Font->Style = TFontStyles() << fsBold;
C->Font->Color = clBlack;

- 메인 지번 노드: 진한 테두리, 굵은 글꼴
- 일반 지번 노드: 회색 테두리, 보통 글꼴
- 변경사유/일자 라벨: 파란 글씨, 작은 폰트

* 스타일용 멤버 변수
int FNodeCornerRadius;
TColor FNodeBorderColor;
TColor FNodeFillColor;
TColor FMainNodeFillColor;
TColor FNodeFontColor;
String FNodeFontName;
int FNodeFontSize;
bool FNodeFontBold;
*/


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


