#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <functional>
#include <iostream>
using namespace FlexAppRules;
using namespace std;

#define FLEX_OMRULE_CALSPD	"CALSPD"

#define strcmpi strcasecmp
#define stricmp strcasecmp

#define CLNT_CPCODE_CM_FLID 1281
#define CLNT_CPCODE_FO_FLID 1282
#define QTY_OFFSET	1
#define LEGID_1 "1"
#define LEGID_2 "2"

#ifdef PLATFORM_WIN
        #include "customer.h"
#else
	#include "/home/demo7app/flex/config/customer.h"
#endif


// SHFE DF/OR Specific: Column names
#ifndef _FUTURE_OR_CLMNAMES_
#define _FUTURE_OR_CLMNAMES_
#define CLMNAME_TICK_SIZE                                                               "TICK_SIZE"
#define CLMNAME_AVAIL                                                                   "AVAIL"
#define CLMNAME_LOT_SIZE                                                                "LOT_SIZE"
#define CLMNAME_ERRCODE_FEED                                                    "ERRCODE_FEED"
#define CLMNAME_MULTIPLIER_FEED                                                 "MULTI_FEED"
#define CLMNAME_POS_YD_LONG                                                             "POS_YD_LONG"
#define CLMNAME_POS_TD_LONG                                                             "POS_TD_LONG"
#define CLMNAME_POS_YD_SHORT                                                    "POS_YD_SHORT"
#define CLMNAME_POS_TD_SHORT                                                    "POS_TD_SHORT"
#define CLMNAME_POS_ABS                                                                 "POS_ABS"
#define CLMNAME_POS_LIMIT                                                               "POS_LIMIT"
#define CLMNAME_L_MARGIN_R                                                              "L_MARGIN_R"
#define CLMNAME_S_MARGIN_R                                                              "S_MARGIN_R"
#define CLMNNAME_UNDERLYING                                                             "UNDERLYING"
#define CLMNAME_ROUND_LOT                                                               "ROUND_LOT"
#define CLMNAME_ROUND_PRICE                                                             "ROUND_PRICE"
#define CLMNAME_AVAIL_YD_LONG                                                   "AVAIL_YD_LNG"
#define CLMNAME_AVAIL_TD_LONG                                                   "AVAIL_TD_LNG"
#define CLMNAME_AVAIL_YD_SHORT                                                  "AVAIL_YD_SHT"
#define CLMNAME_AVAIL_TD_SHORT                                                  "AVAIL_TD_SHT"
#endif // ifndef _FUTURE_OR_CLMNAMES_

// SHFE DF/OR Specific: FLID values
#ifndef _FUTURE_OR_FLIDS_
#define _FUTURE_OR_FLIDS_
const int FLID_TICK_SIZE                                                                = 16;
const int FLID_AVAIL                                                                    = 256;
const int FLID_LONG_MARGIN_RATE                                                 = 262;
const int FLID_SHORT_MARGIN_RATE                                                = 263;
const int FLID_LOT_SIZE                                                                 = 524;
const int FLID_ERRCODE_FEED                                                             = 770;
const int FLID_MULTIPLIER_FEED                                                  = 771;
const int FLID_ORD_TYPE_SUPPORT                                                 = 772;
const int FLID_POS_YD_LONG                                                              = 790;
const int FLID_POS_TD_LONG                                                              = 791;
const int FLID_POS_YD_SHORT                                                             = 792;
const int FLID_POS_TD_SHORT                                                             = 793;
const int FLID_POS_ABS                                                                  = 796;
const int FLID_POS_LIMIT                                                                = 800;
const int FLID_UNDERLYING                                                               = 1130;
const int FLID_MIN_LIMIT_ORD_SIZE                                               = 801;
const int FLID_MAX_LIMIT_ORD_SIZE                                               = 802;
const int FLID_MIN_MKT_ORD_SIZE                                                 = 803;
const int FLID_MAX_MKT_ORD_SIZE                                                 = 804;
const int FLID_AVAIL_YD_LONG                                                    = 805;
const int FLID_AVAIL_TD_LONG                                                    = 806;
const int FLID_AVAIL_YD_SHORT                                                   = 807;
const int FLID_AVAIL_TD_SHORT                                                   = 808;
#endif //#ifndef _FUTURE_OR_FLIDS_

// Strategy Tokens For NEW
// 0	1	  2 	3	   4 	  5 	  6		7		 8	  9			10	    11	   12 
//sym1 sym2 side1 side2 pairid portfolio bench qty clipsize valid_pair ratio lotsize legid

#define TOKEN_SYM_1		0
#define TOKEN_SYM_2		1
#define TOKEN_SIDE_1	2
#define TOKEN_SIDE_2	3
#define TOKEN_PAIRID	4
#define TOKEN_PORT		5
#define TOKEN_BENCH		6
#define TOKEN_QTY		7
#define TOKEN_CLIPSIZE	8
#define TOKEN_VALIDPAIR	9
#define TOKEN_RATIO		10
#define TOKEN_LOTZISE	11
#define TOKEN_LEGID		12

// Strategy Tokens for RPL
//  0	   1	   2	 3    4   5     6         7        8		9
//pairId  sym2  side port bench clipsize valid_pair ratio lotsize LegId
#define TOKEN_RPL_PAIRID	0
#define TOKEN_RPL_SYM_2		1
#define TOKEN_RPL_SIDE		2
#define TOKEN_RPL_PORT		3
#define TOKEN_RPL_BENCH		4
#define TOKEN_RPL_CLIPSIZE	5
#define TOKEN_RPL_VALIDPAIR	6
#define TOKEN_RPL_RATIO		7
#define TOKEN_RPL_LOTSIZE	8
#define TOKEN_RPL_LEGID		9


#define CLIENT_MODE (0) // 1 = Client Mode; 0 = Loopback setup
#define SPD_SYM_DIFF_PORT (1) // 1 = Put the third (spread) symbol in a different portfolio linked to the original one

#define LADDER_TEST
#ifdef LADDER_TEST
	#define LADDER_TESTMODE (1)
#else
	#define LADDER_TESTMODE (0)
#endif

/* Parameters for Ladder*/
const long LADDER_ORD = 42;



/* Constants used to identify Exchange names */
// The Data exchange refers to the name of the exchange obtained in the sym-data flid
// The Send exchanges have to be maintained as "SHFE": They are assumed in the algo side to be those
const char * SHFE_DATA_EXCHANGE = "SHFE"; // Assumption?? Or Get from Market Data
const char * SHFE_CUSTOM_EXCHANGE = "SHFE"; // Used while passing parameters
const char * SHFE_SEND_EXCHANGE = "SHFE"; // Used only in VALID_ORDER



int initialized = 0;
char timeString[128];
void stratStampReal(const char *portfolio, const char *symbol, const char *rulename) {
        char blank[] = "";
        if (!initialized) {
                portfolio = blank;
                symbol = blank;
        }
        /* construct a string representing the time */
        time_t myclock = time(NULL);
        struct tm *mytm = localtime(&myclock);
        strftime(timeString,128,"%T", mytm);

        /* print the first part: TIME PORTFOLIO SYMBOL RULENAME */
        printf("[strategy] %s %15s %8s %12s ", timeString, portfolio, symbol, rulename);

}

#define stratStamp(X) stratStampReal(PORTFOLIO, SYM, X)

#define PAIR_ID_PREFIX "pr_"
#define RA_ID_PREFIX "ra_"
#define RA_PORT_PREFIX "RA"


char PAIR_SEEK_H_PARAM_INPUT_0[1024];
char PAIR_SEEK_H_PARAM_INPUT_1[1024];

char PAIR_SET_LMT_PARS_PARAM_INPUT_0[1024];
char PAIR_SET_LMT_PARS_PARAM_INPUT_1[1024];

int PAIR_BUILD_CHECK_PARAM_OUTPUT_0;

int PAIR_SHARES_1 = 0;
int PAIR_SHARES_2 = 0;

static char PAIR_SYM_1[32] = "\0";
static char PAIR_SYM_2[32] = "\0";
static char PAIR_PORT_1[32] = "\0";
static char PAIR_PORT_2[32] = "\0";
static char PAIR_PRICE_1[32] = "\0";
static char PAIR_PRICE_2[32] = "\0";
static char PAIR_BOOK_NAME[32] = "\0";
double PAIR_NUM_PRICE_1 = 0;
double PAIR_NUM_PRICE_2 = 0;

int PAIR_LOT_1 = 1;
int PAIR_LOT_2 = 1;

const int TAKE_CLID = 100;
const int POST_CLID = 10;

// In Lots
#ifndef _MAX_POST_SIZE_DEF_
#define _MAX_POST_SIZE_DEF_
const int MAX_POST_SIZE = 5;
#endif

double dblDefPartRate = 100.00; // Pairs Strategy

int RPL_COUNT;

char szBen[30], szQty[30], szSize[30];

class SodPosition
{
	public:
		SodPosition(const char* pszLine)
		{
			m_sSymbol = m_sPortfolio = m_sTrader = "";
			m_bIsBuy = true;
			m_nExecQty = 0; m_dAvgPrice = 0;
			ParseLine(pszLine);
		}
	
	public:
		const char* Symbol() { return m_sSymbol.c_str();}
		const char* Portfolio() { return m_sPortfolio.c_str();}
		const char* Trader() { return m_sTrader.c_str();}
		int ExecQty() { return m_nExecQty;}
		double AvgPrice() { return m_dAvgPrice;}
		bool IsBuy() { return m_nExecQty >= 0;}
		const char* PAIR_ID() { return m_sPAIR_ID.c_str();}
		const char* TARGET_SIZE() { return m_sTARGET_SIZE.c_str();}
		const char* PAIR_SYM() { return m_sPAIR_SYM.c_str();}
		const char* PAIR_PORT() { return m_sPAIR_PORT.c_str();}
		const char* SLLBY_SPREAD() { return m_sSLLBY_SPREAD.c_str();}
		const char* ACCOUNT() { return m_sACCOUNT.c_str();}
		const char* CLIPSIZE() { return m_sCLIPSIZE.c_str();}
		const char* PAIR_SIDE() { return m_sPAIR_SIDE.c_str();}
		const char* ORDER_SIDE() { return m_sORDER_SIDE.c_str();}
	
	private:
		std::string m_sSymbol, m_sPortfolio, m_sTrader, m_sPAIR_ID, m_sTARGET_SIZE,\
			m_sPAIR_SYM, m_sPAIR_PORT, m_sSLLBY_SPREAD, m_sACCOUNT, m_sCLIPSIZE, m_sPAIR_SIDE, m_sORDER_SIDE;

		bool m_bIsBuy;
		int m_nExecQty;
		double m_dAvgPrice;
		
	public:
		static void split(const string& s, char c,vector<string>& v) {
		string::size_type i = 0;
		string::size_type j = s.find(c);
		while (j != string::npos) {
			v.push_back(s.substr(i, j-i));
			i = ++j;
			j = s.find(c, j);
			if (j == string::npos)
			v.push_back(s.substr(i, s.length( )));
		}
	}
	
	private:
	void ParseLine(const char* pszLine){
		vector<string> v;
		std::string s = pszLine;
		split(s, ',', v);
		for (int i = 0; i < v.size( ); ++i) {
			int j = i+1;
			switch(j){
				case 1:
					m_sTrader = v[i];
					break;
				case 2:
					m_sPortfolio = v[i];
					break;
				case 3:
					m_sSymbol = v[i];
					break;
				case 4:
					m_nExecQty = atoi(v[i].c_str());
					break;
				case 5:
					m_dAvgPrice = atof(v[i].c_str());
					break;
				case 6:	
					m_sPAIR_ID = v[i]; 
					break;
				case 7:	
					m_sTARGET_SIZE = v[i];
					break;
				case 8:
					m_sPAIR_SYM = v[i]; 
					break;
				case 9:
					m_sPAIR_PORT = v[i];
					break;
				case 10:
					m_sSLLBY_SPREAD = v[i]; 
					break;
				case 11:
					m_sACCOUNT = v[i]; 
					break;
				case 12:
					m_sCLIPSIZE = v[i];
					break;
				case 13:
					m_sPAIR_SIDE = v[i];
					break;
				case 14:
					m_sORDER_SIDE = v[i]; 
					break;
				default:
					break;
			}
		}
	}
};

bool _Is_My_Position(const char* pszTrader, SodPosition* pPosition)
{
	if(!pszTrader || !pPosition)
		return false;
	
	return (0 == strcmp(pszTrader, pPosition->Trader()));
}

#define IsMyPosition(p) _Is_My_Position(TraderName(), p)

std::map<std::string, SodPosition* > g_mSodPositions;
std::map<std::string, SodPosition* >::iterator g_iSodPositions;

void StoreSodPosition(SodPosition *pPosition)
{
	if(!pPosition)
		return;
	std::string key(pPosition->Portfolio());
	key += pPosition->Symbol();
	g_mSodPositions[key] = pPosition;
}

SodPosition* GetSodPosition(const char* pszKey)
{
	SodPosition *pPosition = NULL;
	if(pszKey)
	{
		g_iSodPositions = g_mSodPositions.find(pszKey);
		if(g_iSodPositions != g_mSodPositions.end())
			pPosition = g_iSodPositions->second;
	}
	return pPosition;
}

void SubscrSymServer(const char* sy, bool b);
CPriceInfo* FindPriceInfo(const char * s, int c);


/* SPREAD SPECIFIC UTILITIES */

const char * GetMWTradeFileName()
{
	static char szTradeFile[32]="";
	int iCounter = 1000;
	bool bMatchFound = false;
	while(!bMatchFound)
	{
		sprintf(szTradeFile, "MW_%s_%d", TraderName(), iCounter);
		if(!Is_Port_Loaded(szTradeFile))
			bMatchFound = true;
		iCounter++;
	}
	return szTradeFile;
}

const char * FixedMWPort()
{
	static char szPort[32] ="";
	if(szPort[0] == '\0')
		sprintf(szPort,"WatchList_%s",TraderName());
	return szPort;
}

const long SPD_DUMMY_ORD_ID = 1331;
const char * GLOBAL_DELIMITER_STRING = "-";
const char GLOBAL_DELIMITER_CHAR = '-';

const char * GetSpreadSymbol(const char * pszFirstSym, const char * pszSecondSym, char cDelimiter = GLOBAL_DELIMITER_CHAR)
{
	char szSpreadSymbol[32] = "";
	
	if(pszFirstSym && pszSecondSym)
		sprintf(szSpreadSymbol,"%s%c%s",pszFirstSym,cDelimiter,pszSecondSym);
	else
		cerr << " Unable to build spread symbol with First Sym = " << pszFirstSym << " and Second Sym = " << pszSecondSym << endl;
		
	cerr << "New Spread symbol = " << szSpreadSymbol<<endl;	
	
	return szSpreadSymbol;
}

const char * ORD_TYPES[] = {"MKT", "LIM", "IOC", "NONE"};

std::string _STR_LINKED_PORT_;
void SetLinkedPort(const char * pszPort)
{
	if(pszPort)
		_STR_LINKED_PORT_ = pszPort;
}
const char * GetLinkedPort()
{
	return _STR_LINKED_PORT_.c_str();
}
void ClearLinkedPort()
{
	_STR_LINKED_PORT_.clear();
}

typedef enum{
	enColumn1 = 33,
	enColumn2 = 16,
	enColumn3 = 2,
	enColumn4 = 15,
	enColumn5 = 34,
	enInvalidColumn = -1
}SS_COLUMN_NUMBER;

typedef enum{
	enSingleClick,
	enDoubleClick,
	enRightClick,
	enMiddleClick,
	enShiftLeftClick,
	enCtrlLeftClick,
	enOtherClick
}SS_MOUSE_CLICK;

std::string strSpdLadderPort = "";
bool bIsSpdOrderFiringPending = false;
std::string strGlobalMemo = "";


/*
 *List of ENUMS : Gold and Copper
 */

typedef enum enCMInstrType
{
        enCopper,
        enGold,
        enSilver,
        enCSI300,
        enNoInstr
};


enCMInstrType GetInstrType(int i)
{
        switch(i)
        {
                case 0:
                        return enCopper;
                case 1:
                        return enGold;
                case 2:
                        return enSilver;
                case 3:
                		return enCSI300;
                default:
                        return enNoInstr;
        }
}

const char* GetProductName(enCMInstrType eType)
{
        static const char* p[5] = {"COPPER","GOLD", "SILVER", "CSI300", "NONE"};
        switch(eType)
        {
                case enCopper:
                        return p[0];
                case enGold:
                        return p[1];
                case enSilver:
                        return p[2];
                case enCSI300:
                		return p[3];
                default:
                        return p[4];
        }
        return p[4];
}


const char* GetSHFEProductSymbol(enCMInstrType eType)
{
        static const char* p[5] = {"CU","AU", "AG", "IF", "NONE"};
        switch(eType)
        {
                case enCopper:
                        return p[0];
                case enGold:
                        return p[1];
                case enSilver:
                        return p[2];
                case enCSI300:
                		return p[3];
                default:
                        return p[4];
        }
        return p[4];
}


enCMInstrType GetInstrType(const char* pszSymbol)
{
        if(!pszSymbol)
                return enNoInstr;

        if(strstr(pszSymbol, "AU"))
                return enGold;
        else if(strstr(pszSymbol, "CU"))
                return enCopper;
        else if(strstr(pszSymbol, "AG"))
                return enSilver;
        else if(strstr(pszSymbol, "IF"))
                return enCSI300;
        return enNoInstr;
}

enCMInstrType GetInstrTypeFromProduct(const char* pszProduct)
{
        if(!pszProduct)
                return enNoInstr;

        if(!strcmp(pszProduct,"GOLD"))
                return enGold;
        else if(!strcmp(pszProduct,"COPPER"))
                return enCopper;
        else if(!strcmp(pszProduct,"SILVER"))
                return enSilver;
        else if(!strcmp(pszProduct,"CSI300"))
                return enCSI300;
        return enNoInstr;
}

const char* GetProductFromSymbol(const char* pszSymbol)
{
        return GetProductName(GetInstrType(pszSymbol));
}

/*
 * Create WatchList
 */

void LoadCommodityWatchList(int iInstrType, int nWatchMonth, int iCurrentDate)
{
	int iCurrentYear = 0, iCurrentMonth = 0, iStartMonth=0, iMonth=0;
	static enCMInstrType eInstrType = enNoInstr;
	char szExpiry[32] = "", szCommodity[32] = "";

	eInstrType = GetInstrType(iInstrType);
	sprintf(szCommodity,GetSHFEProductSymbol(eInstrType));

	iCurrentYear = iCurrentDate/10000;
	iCurrentMonth = (iCurrentDate/100)%100;
	iStartMonth = int ((iCurrentDate%100)/16);

	for(int i=iStartMonth;i<nWatchMonth+iStartMonth; i++)
	{
		iMonth = iCurrentMonth + i;
		if(iMonth > 12)
		{
			iMonth %= 12;
			sprintf(szExpiry, "%s%02d%02d", szCommodity,(iCurrentYear+1)%100,iMonth%12);
		}
		else
		{
			sprintf(szExpiry, "%s%02d%02d", szCommodity,iCurrentYear%100,iMonth);
		}
		Add_Symbol_Buy(FixedMWPort(), szExpiry, 0);
	}
}

// Calculate Day of the Week
// Tondering's algorithm
int DayOfWeek (int y, int m, int d)
{
	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	y -= m < 3;
	return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

void LoadCSI300WatchList(int iCurrentDate)
{
	int iCurrentYear = 0, iCurrentMonth = 0, iDOWFirstDay=0, iDeliveryDay=0;
	int iStartMonth=0, iMonth=0, iQuarter=0;
	static enCMInstrType eInstrType = enNoInstr;
	char szExpiry[32] = "", szCommodity[32] = "";

	int iInstrType = 3;
	eInstrType = GetInstrType(iInstrType);
	sprintf(szCommodity,GetSHFEProductSymbol(eInstrType));

	iCurrentYear = iCurrentDate/10000;
	iCurrentMonth = (iCurrentDate/100)%100;
	iDOWFirstDay = DayOfWeek(iCurrentYear, iCurrentMonth, 1);
	iDeliveryDay = iDOWFirstDay<5? (20-iDOWFirstDay) : 21;
	iStartMonth = int ( (iCurrentDate%100)>iDeliveryDay ? 1 : 0);

	// Current month and next month
	for(int i=iStartMonth;i<2+iStartMonth; i++)
	{
		iMonth = iCurrentMonth + i;
		if(iMonth > 12)
		{
			iMonth %= 12;
			sprintf(szExpiry, "%s%02d%02d", szCommodity,(iCurrentYear+1)%100,iMonth%12);
		}
		else
		{
			sprintf(szExpiry, "%s%02d%02d", szCommodity,iCurrentYear%100,iMonth);
		}
		Add_Symbol_Buy(FixedMWPort(), szExpiry, 0);
	}

	// Next two calendar quarters
	iQuarter = (iCurrentMonth+iStartMonth+1)/3;
	for (int i=1; i<3; i++)
	{
		iMonth = (iQuarter+i)*3;
		if(iMonth > 12)
		{
			iMonth %= 12;
			sprintf(szExpiry, "%s%02d%02d", szCommodity,(iCurrentYear+1)%100,iMonth%12);
		}
		else
		{
			sprintf(szExpiry, "%s%02d%02d", szCommodity,iCurrentYear%100,iMonth);
		}
		Add_Symbol_Buy(FixedMWPort(), szExpiry, 0);
	}
}

void LoadBoDWatchList()
{
	int nWatchMonth = GlobalValue("WATCHMONTH");

	if(!Is_Port_Loaded(FixedMWPort()))
	{
		char szCurrentDate[32] = "";
		int iCurrentDate = 0, iInstrType=0;
		Current_Date6(szCurrentDate);
		iCurrentDate = atoi(szCurrentDate);

		// Load Commodities: AU, AG, CU
		for(iInstrType = 0; iInstrType < 3; iInstrType++)
		{
			LoadCommodityWatchList(iInstrType, nWatchMonth, iCurrentDate);
		}

		LoadCSI300WatchList(iCurrentDate);
	}
}

// Get the Available Funds for an Investor
#ifndef _GET_AVAL_FUND_
#define _GET_AVAL_FUND_
double GetInvestorAvailFund(const char *pszSym)
{
        if (!pszSym)
                return 0;
        FlexAppRules::MarketInfo MktInfo(pszSym);
        //if(MktInfo.HasReceivedMarketData())
        //{
                return MktInfo.GetFlid(FLID_AVAIL, 0.0);
        //}
        //return 0;
}
#endif
