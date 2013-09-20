#ifndef _CalendarSpread_H_
#define _CalendarSpread_H_

#include <RECommon.h>
#include <REHostExport.h>
#include <string.h>
#include "boost/unordered_map.hpp"

#ifndef _STRATCOMMON_FIXTAG_H_
#include "../Common/fixtag.h"
#endif

#ifndef _STRATCOMMON_LOGGER_H_
#include "../Common/logger.h"
#endif

namespace FlexStrategy
{
namespace CalendarSpread
{
// Included in ../Common/fixtag.h
/*
// FIX_TAG_STRATEGYNAME 9500
#define FIX_TAG_DESTINATION 9501
#define FIX_TAG_OMUSER 9502
#define FIX_TAG_CLIENTCODE 9511
#define FIX_TAG_CPCODE 9512
#define FIX_TAG_CLIP_SIZE 9521
#define FIX_TAG_PAIRID 9522
#define FIX_TAG_PAIRSYM 9523
#define FIX_TAG_BENCHMARK 9524
#define FIX_TAG_PLAY_PAUSE 9269
#define FIX_TAG_RATIO 9524
*/
#define STR_LEN 32
#define STR_LEN_LONG 128
#define QTY_OFFSET 1
//#define TICK_SIZE 0.05
#define LEGID_1 1
#define LEGID_2 2

static const double dEpsilon = 0.0001;
static const char cDelimiter = ';';

struct PairParams
{
	long nPairId;
	char szSymbol1[STR_LEN]; //Buy Symbol
	char szSymbol2[STR_LEN]; //Sell Symbol
	char szClientOrdId1[STR_LEN];	
	char szClientOrdId2[STR_LEN];	
	char szPortfolio1[STR_LEN];
	char szPortfolio2[STR_LEN];	
	double dBenchmark;
	int nClipSize1;
	int nClipSize2;
	double dRatio1;
	double dRatio2;
	int nLotSize1;
	int nLotSize2;
	enOrderSide ensSide1;
	enOrderSide ensSide2;
	// EXCH
	char szOMUser[STR_LEN];
	char szClientCode[STR_LEN];
	char szCpCode[STR_LEN];
	char szDest[STR_LEN];
	bool bPairReady;
	bool bPairRunning;
	// Generic
	unsigned int nEntryCount;
	bool bCliRpld1;
	bool bCliRpld2;
	//MktData
	int nContraSizePrev1;
	double dContraPricePrev1;
	int nContraSizePrev2;
	double dContraPricePrev2;

	PairParams()
	{
		nPairId=0;
		strcpy(szSymbol1,"");
		strcpy(szSymbol2,"");
		strcpy(szClientOrdId1,"");
		strcpy(szClientOrdId2,"");
		strcpy(szOMUser,"");
		strcpy(szClientCode,"");
		strcpy(szCpCode,"");
		strcpy(szDest,""); 
		dBenchmark=0.0;
		nClipSize1=0;
		nClipSize2=0;
		dRatio1=1;
		dRatio2=1;
		nLotSize1=0;
		nLotSize2=0;
		bPairReady=false;
		bPairRunning=false;
		nEntryCount = 0;
		bCliRpld1 = false;
		bCliRpld2 = false;
		nContraSizePrev1=0;
		dContraPricePrev1=0.0;
		nContraSizePrev2=0;
		dContraPricePrev2=0.0;
	}
	void print()
	{cout<<"1:"<<szSymbol1<<" 2:"<<szSymbol2<<" Oid1:"<<szClientOrdId1<<" Oid2:"<<szClientOrdId2<<endl;}

};

//typedef std::map<long,PairParams*> PAIRMAP;
//typedef std::map<long,PairParams*>::iterator PAIRMAPITER;
typedef boost::unordered_map<long,PairParams*> PAIRMAP;
typedef boost::unordered_map<long,PairParams*>::iterator PAIRMAPITER;
//TEMP FIX
//typedef std::map<std::string,long> PAIRIDMAP;
//typedef std::map<std::string,long>::iterator PAIRIDMAPITER;
typedef boost::unordered_map<std::string,long> PAIRIDMAP;
typedef boost::unordered_map<std::string,long>::iterator PAIRIDMAPITER;

class CalendarSpread : public StrategyBase
{
public:

	enum E_POR_RET {
		POR_RET_ERROR = -1,
		POR_RET_FAILURE = 0,
		POR_RET_SUCCESS
	};

	CalendarSpread();
	virtual ~CalendarSpread();

	//Market Data event implementation
	int OnMarketData(CLIENT_ORDER& ClientOrder, MTICK& MTick);
	int OnMarketData(MTICK& MTick);
	int OnLoad(const char* szConfigFile = NULL);

	// Connection call backs
	int OnClientConnect(const char* szDest);
	int OnClientDisconnect(const char* szDest);

	int OnStreetConnect(const char* szDest);
	int OnStreetDisconnect(const char* szDest);

	int OnConfigUpdate(const char* szConfigBuf);
	int OnClientCommand(STRAT_COMMAND &pCommand);

	int OnEndOfRecovery();

	//Following are on Client events for his particular orders
	int OnClientOrdNew(CLIENT_ORDER& ClientOrd);
	int OnClientOrdRpl(CLIENT_ORDER& ClientOrd);
	int OnClientOrdCancel(CLIENT_ORDER& ClientOrd);
	int OnClientSendAck(CLIENT_EXEC& ClientOrd);
	int OnClientSendOut(CLIENT_EXEC& ClientOrd);
	int OnClientSendReject(CLIENT_EXEC& ClientExec);
	int OnClientSendFills(CLIENT_EXEC& ClientExec);
	int OnClientOrdValidNew(CLIENT_ORDER& ClientOrd);
	int OnClientOrdValidRpl(CLIENT_ORDER& ClientOrd);
	int OnClientOrdValidCancel(CLIENT_ORDER& ClientOrd);

	// Street Orders
	int OnStreetOrdNew(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdRpl(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdCancel(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdAck(STREET_ORDER& StreetOrd);
	int OnStreetOrdRej(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdOut(STREET_ORDER& StreetOrd);
	int OnStreetExec(STREET_EXEC& StreetExec, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdValidNew(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdValidRpl(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	int OnStreetOrdValidCancel(STREET_ORDER& StreetOrd, CLIENT_ORDER& ParentOrder);
	virtual int OnStreetStatusReport(STREET_EXEC& StreetExec, CLIENT_ORDER& ParentOrder) { return SUCCESS;}                 							
	int OnClientSendPendingRpl(OMRULESEXPORT::CLIENT_EXEC& ClientExec);

	int OnTimer(EVENT_DATA& eventData);

	//Custom
	int ValidateFIXTags(ORDER& order);
	int AddParamsToMap(ORDER& order,PAIRMAP& pMap);
	int	GetClientOrder(CLIENT_ORDER& QuoteOrder, PairParams *pPairParams, bool bBuyLeg);
	int CancelUnfilledOrders(PairParams *pPairParams);
	bool CalculateOrderSize(int iLeg1LotSize, int iLeg2LotSize, double dLeg1Ratio, double dLeg2Ratio, int iLeg1MktSize, int iLeg2MktSize, int iLeg1AvailQty, int iLeg2AvailQty, int& iLeg1OrderQty, int& iLeg2OrderQty);
	int CancelAllOrdersForClientOrder(const char* pszClientOrderId);
	bool CanReplace(STREET_ORDER& StreetOrder);
	const char* ErrorCodeToStr(enErrorCode e);
	const char* StateToStr(enOrderState e);

private:
	void TestLevel2(std::string symbol);
	const CalendarSpread::E_POR_RET CancelBothStreetOrders(const PairParams *pPairParams);
	void PrintOrderMap(PAIRMAP& mpPairMap, const long nPairId);
	void PrintOrderMapRpl(PAIRMAP& mpPairMap);

private:
	static char *m_sFixTagStrategyName;
	static std::string m_sPropertiesFileName;
	static std::string m_sFixDestination;

	static int m_sCount;
	static int m_sStreetOrderSize;
};

static const unsigned int	C_CALSPD_NUM_OF_LEGS_INDEX	= 1; // Num of legs = 2: [0,1]
static const unsigned int	C_CALSPD_NUM_OF_LEGS		= 2; // Num of legs = 2

extern PAIRMAP gmPairMap, gmPairMapRpl;
extern PAIRMAP gmPairMapCxl;
extern PAIRIDMAP gmPairIdMap;

}; // namespace CalendarSpread
}; // namespace FlexStrategy

#endif //_CalendarSpread_H_
