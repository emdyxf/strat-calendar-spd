#include "./CalendarSpread.h"
#include <sstream>
#include <map>
#include <sys/time.h>

using namespace FlexStrategy::CalendarSpread;

char *CalendarSpread::m_sFixTagStrategyName = "CALSPD";

PAIRMAP FlexStrategy::CalendarSpread::gmPairMap, FlexStrategy::CalendarSpread::gmPairMapRpl;
PAIRMAP FlexStrategy::CalendarSpread::gmPairMapCxl;
PAIRIDMAP FlexStrategy::CalendarSpread::gmPairIdMap;

CalendarSpread::CalendarSpread() :
	StrategyBase(m_sFixTagStrategyName, LOG_ERROR)
{
	CINFO << "----------------------------------------------------------------------" << std::endl;
	CINFO << " In CalendarSpread CTOR strat name = " << GetStratName() << std::endl;
	CINFO << " Build Date [" << BUILD_DATE << "]" << std::endl;
	CINFO << " Build Time [" << BUILD_TIME << "]" << std::endl;
	CINFO << " Build Version [" << BUILD_VERSION << "]" << std::endl;

	switch (STRAT_DEBUG_LEVEL)
	{
		case 0:		CINFO << " Logging Level: Info " << std::endl;		break;
		case 1:		CINFO << " Logging Level: Verbose " << std::endl;		break;
		default:	CINFO << " Logging Level: Debug " << std::endl;		break;
	}
	CINFO << "----------------------------------------------------------------------" << std::endl;
}

CalendarSpread::~CalendarSpread()
{
	CINFO << __PRETTY_FUNCTION__<< std::endl;
}

int CalendarSpread::OnLoad(const char* szConfigFile)
{
	CINFO << __PRETTY_FUNCTION__<< std::endl;
	SetSignalEnabled(false);
	return SUCCESS;
}

int CalendarSpread::OnEndOfRecovery(){return SUCCESS;}

int CalendarSpread::OnMarketData(CLIENT_ORDER& order, MTICK& mtick){

	// STEP 1 : Get the Pair Paramaters from the structure	
	_STRAT_LOG_DBUG_(CDEBUG << "Process Market Data with order [" << order.GetHandle() << "]"  << std::endl);

	PAIRIDMAPITER mIdIter = gmPairIdMap.find(string(order.GetHandle()));
	long nPairId = -1;
	if(mIdIter != gmPairIdMap.end())
	{
		nPairId = (long) mIdIter->second;
		_STRAT_LOG_DBUG_(CDEBUG << "Found Pair [" << order.GetHandle() << "]"  << std::endl);
	}
	else
	{
		_STRAT_LOG_VERB_(CWARN << "Order [" << order.GetHandle() << "] not found in Map!" << std::endl);
		return FAILURE;
	}

	PAIRMAPITER mIter = gmPairMap.find(nPairId);
	PairParams *pPairParams = NULL;
	if(mIter != gmPairMap.end())
		pPairParams = (PairParams*) mIter->second;
	else
	{
		_STRAT_LOG_VERB_(CWARN << "Pair [" << nPairId << "] not found in gmPairMap!" << std::endl);
		return FAILURE;
	}

	if(!pPairParams)
	{
		_STRAT_LOG_VERB_(CWARN << "OnMD Debug: Invalid PairParam!" << std::endl);
		return FAILURE;
	}
	
	if(! ((pPairParams->bPairReady)&&(pPairParams->bPairRunning)))
	{
		_STRAT_LOG_VERB_(CWARN << "PairID [" << nPairId
								<< "] is stopped: isReady? [" << pPairParams->bPairReady
								<< "] isRuning? [" << pPairParams->bPairRunning
								<< "]"
								<< std::endl);
		return FAILURE;
	}

	// Cancel Unfilled Orders - IOC operation
	CancelUnfilledOrders(pPairParams);

	if(order.GetUnOrdSize() < (QTY_OFFSET + 1)) //For checking minimum quantity
	{
		_STRAT_LOG_DBUG_(CDEBUG << "No Quantity! UnordSize [" << order.GetUnOrdSize() << "]" << std::endl);

		return FAILURE;
	}

	_STRAT_LOG_DBUG_(CDEBUG << "Proceed to get market data." << std::endl);
	
	// STEP 2 : Get the relevant market data fields
	MTICK mtick1, mtick2;
	if(!strcmp(order.GetSymbol(),pPairParams->szSymbol1))
	{
		mtick1 = mtick;
		MarketDataGetSymbolInfo(pPairParams->szSymbol2, mtick2);
	}
	else if(!strcmp(order.GetSymbol(),pPairParams->szSymbol2))
	{
		mtick2 = mtick;
		MarketDataGetSymbolInfo(pPairParams->szSymbol1, mtick1);
	}
	else
	{
		_STRAT_LOG_DBUG_(CDEBUG << "Failed to get relevant market data fields!"<< std::endl);
		return FAILURE;
	}

	double dContraPrice1 = 0.0,dContraPrice2=0.0,dFactor1=1.0,dFactor2=1.0;
	int dContraSize1 = 0 , dContraSize2 = 0 ;

	if(pPairParams->ensSide1 == SIDE_BUY)
	{
		dContraPrice1 = mtick1.GetAsk();
		dContraSize1 = mtick1.GetAskSize();
		dFactor1 = -1.0; //Buy has a factor of -1 for use in spread calculation
	}
	else if(pPairParams->ensSide1 == SIDE_SELL)
	{
		dContraPrice1 = mtick1.GetBid();
		dContraSize1 = mtick1.GetBidSize();
	}

	if(pPairParams->ensSide2 == SIDE_BUY)
	{
		dContraPrice2 = mtick2.GetAsk();
		dContraSize2 = mtick2.GetAskSize();
		dFactor2 = -1.0;
	}
	else if(pPairParams->ensSide2 == SIDE_SELL)
	{
		dContraPrice2 = mtick2.GetBid();
		dContraSize2 = mtick2.GetBidSize();
	}

	if(dContraPrice1 < dEpsilon || dContraPrice2 < dEpsilon)
	{
		_STRAT_LOG_VERB_(CERR << "Invalid Prices in market data! Symbol1 [" << mtick1.GetSymbol()
				<< "] Bid [" << mtick1.GetBid()
				<< "] Ask [" << mtick1.GetAsk()
				<< "] : Price1 [" << dContraPrice1
				<< "] - Symbol2 [" << mtick2.GetSymbol()
				<< "] Bid [" << mtick2.GetBid()
				<< "] Ask [" << mtick2.GetAsk()
				<< "] Price2 [" << dContraPrice2 << "]"
				<< std::endl);
		return FAILURE;
	}

	// STEP 3 : Check if relevent market data fields have been updated before proceeding
	if((fabs(pPairParams->dContraPricePrev1-dContraPrice1)) > dEpsilon || (fabs(pPairParams->dContraPricePrev2-dContraPrice2) > dEpsilon) || ((pPairParams->nContraSizePrev1 - dContraSize1) != 0) || ((pPairParams->nContraSizePrev2 - dContraSize2 )!= 0))
	{
		//Update the market data snapshot
		pPairParams->dContraPricePrev1 = dContraPrice1;
		pPairParams->dContraPricePrev2 = dContraPrice2;
		pPairParams->nContraSizePrev1 = dContraSize1;
		pPairParams->nContraSizePrev2 = dContraSize2;
	}
	else  
	{
		_STRAT_LOG_DBUG_(CDEBUG << "Market data not updated!"<< std::endl);
		return FAILURE;
	}
	// STEP 4 : Calculate the Market Spread
	double dMktSpread = 0.0;
	dMktSpread = ( pPairParams->dRatio1 * dContraPrice1 * dFactor1 ) + ( pPairParams->dRatio2 * dContraPrice2 * dFactor2 );

	// STEP 5 : Check the Market Spread against benchmark and Adjust
	double dTickSize = 0.0;
	dTickSize = mtick.GetTickSize();
	if (dTickSize < dEpsilon)
	{
		_STRAT_LOG_VERB_(CERR << "Invalide Tick Size [" << dTickSize
				<< "] for Symbol [" << mtick.GetSymbol()
				<< "]!"
				<< std::endl);
		return FAILURE;
	}

	if((dMktSpread-pPairParams->dBenchmark) > (-dEpsilon))
	{
		//Adjust to acheive the benchmark spread
		int nQuot = int(((dMktSpread - pPairParams->dBenchmark ) / ( (pPairParams->dRatio1 + pPairParams->dRatio2) * dTickSize)) + dEpsilon);
		dContraPrice1 = dContraPrice1 + ( nQuot * dTickSize * -1 * dFactor1 ) ;
		dContraPrice2 = dContraPrice2 + ( nQuot * dTickSize * -1 * dFactor2 ) ;

		if((int(pPairParams -> dRatio1) == 1) && (int(pPairParams->dRatio2) == 1))
		{
			int nRem = int((((dMktSpread - pPairParams->dBenchmark ) - ( (pPairParams->dRatio1 + pPairParams->dRatio2) * nQuot * dTickSize)) / dTickSize) + dEpsilon) ;
			dContraPrice2 = dContraPrice2 + ( nRem * dTickSize * -1 * dFactor2);
		}
	}
	else
	{
		_STRAT_LOG_VERB_(CWARN << "Market Spread [" << dMktSpread
				<< "] LESS than Required Spread [" <<  pPairParams->dBenchmark << "]"
				<< std::endl);
		return FAILURE;
	}
	// STEP 6 : Determine the leg order sizes
	int nOrderQty1=0,nOrderQty2=0;
	CLIENT_ORDER clntOrdOther;
	if(!strcmp(order.GetSymbol(),pPairParams->szSymbol1))
        {
		if (!GetClientOrderById(clntOrdOther, pPairParams->szClientOrdId2))
               		return FAILURE;
		if(!CalculateOrderSize(pPairParams->nLotSize1,pPairParams->nLotSize2, pPairParams->dRatio1, pPairParams->dRatio2, dContraSize1, dContraSize2, order.GetUnOrdSize() - QTY_OFFSET, clntOrdOther.GetUnOrdSize() - QTY_OFFSET, nOrderQty1, nOrderQty2))
			return FAILURE;
	}
	else if(!strcmp(order.GetSymbol(),pPairParams->szSymbol2))
        {
		if (!GetClientOrderById(clntOrdOther, pPairParams->szClientOrdId1))
               		return FAILURE;
		if(!CalculateOrderSize(pPairParams->nLotSize1,pPairParams->nLotSize2, pPairParams->dRatio1, pPairParams->dRatio2, dContraSize1, dContraSize2, clntOrdOther.GetUnOrdSize() - QTY_OFFSET, order.GetUnOrdSize() - QTY_OFFSET, nOrderQty1, nOrderQty2))
			return FAILURE;
	}
	else
		return FAILURE;

        //Limit Order Size to Clip Size if required
        nOrderQty1 = ((nOrderQty1 > int(pPairParams->dRatio1 * pPairParams->nClipSize1)) ? int(pPairParams->nClipSize1 *pPairParams->dRatio1) : nOrderQty1);
        nOrderQty2 = ((nOrderQty2 > int(pPairParams->dRatio2 * pPairParams->nClipSize2)) ? int(pPairParams->nClipSize2 *pPairParams->dRatio2) : nOrderQty2);

	if(nOrderQty1 == 0 || nOrderQty2 == 0) //Street Order Params Validation
		return FAILURE;
	
	if(!IsDestinationUp(pPairParams->szDest))
		return FAILURE;

	// STEP 7 :  Send the Street Order
	//Buy Order Leg
        STREET_ORDER soB;
        soB.SetClientOrdId(pPairParams->szClientOrdId1);
        soB.SetSymbol(pPairParams->szSymbol1);
        soB.SetSide(pPairParams->ensSide1);
        soB.SetPrice(dContraPrice1);
        soB.SetSize(nOrderQty1);
        soB.SetDestination(pPairParams->szDest);
        soB.SetOrderTimeInForce(TIF_DAY);
        soB.SetAccount(pPairParams->szClientCode);
        soB.SetFixTagStr(pPairParams->szCpCode);
		_STRAT_LOG_VERB_(CWARN << "Pair ID [" << order.GetFixTag(FIX_TAG_PAIRID_PR) << "] Leg BUY" << std::endl);
        soB.SetWaveName(order.GetFixTag(FIX_TAG_PAIRID_PR));
        bool bRet = soB.Send();
     //Sell Order Leg
        STREET_ORDER soS;
        soS.SetClientOrdId(pPairParams->szClientOrdId2);
        soS.SetSymbol(pPairParams->szSymbol2);
        soS.SetSide(pPairParams->ensSide2);
        soS.SetPrice(dContraPrice2);
        soS.SetSize(nOrderQty2);
        soS.SetDestination(pPairParams->szDest);
        soS.SetOrderTimeInForce(TIF_DAY);
        soS.SetAccount(pPairParams->szClientCode);
        soB.SetFixTagStr(pPairParams->szCpCode);
		_STRAT_LOG_VERB_(CWARN << "Pair ID [" << order.GetFixTag(FIX_TAG_PAIRID_PR) << "] Leg SEL" << std::endl);
        soS.SetWaveName(order.GetFixTag(FIX_TAG_PAIRID_PR));
        bRet = soS.Send();

	struct timeval curr_tv = {0,0};
	if (-1 == gettimeofday(&curr_tv, NULL))
		CINFO << "CALSPD: Get time of day did not work" << endl;

	dMktSpread = ( pPairParams->dRatio1 * dContraPrice1 * dFactor1 ) + ( pPairParams->dRatio2 * dContraPrice2 * dFactor2 );

	cout<< "Order sent at spread : " << dMktSpread <<"|"<<nPairId<<"|"<<curr_tv.tv_sec<<"|"<<curr_tv.tv_usec<<std :: endl;	

	CINFO << "[" << curr_tv.tv_sec<<"|"<<curr_tv.tv_usec <<"]"
			<< " Order sent at spread :"
			<< " MktSpread ["<< dMktSpread
			<< " ] PairID [" << nPairId
			<< " ]"
			<< std::endl;
	
	return SUCCESS;
}

int CalendarSpread::OnMarketData(MTICK& MTick){return SUCCESS;}

// Connection call backs
int CalendarSpread::OnClientConnect(const char* szDest)
{
	CINFO << __PRETTY_FUNCTION__ << std::endl;
	return SUCCESS;
}

int CalendarSpread::OnClientDisconnect(const char* szDest)
{
	CINFO << __PRETTY_FUNCTION__ << std::endl;
	return SUCCESS;
}

int CalendarSpread::OnStreetConnect(const char* szDest)
{
	CINFO << __PRETTY_FUNCTION__ << std::endl;
	return SUCCESS;
}

int CalendarSpread::OnStreetDisconnect(const char* szDest)
{
	CINFO << __PRETTY_FUNCTION__ << std::endl;
	return SUCCESS;
}

int CalendarSpread::OnConfigUpdate(const char* szConfigBuf)
{
	CINFO << __PRETTY_FUNCTION__ << " Got string buf = " << szConfigBuf << std::endl;
	return SUCCESS;
}

int CalendarSpread::OnClientCommand(STRAT_COMMAND &pCommand)
{
	CINFO << __PRETTY_FUNCTION__ << std::endl;
	return SUCCESS;
}

// Client Order Callbacks
int CalendarSpread::OnClientOrdValidNew(CLIENT_ORDER& order)
{
	return CalendarSpread::ValidateFIXTags(order);
}

int CalendarSpread::OnClientOrdNew(CLIENT_ORDER& order)
{
	long nPairId = atol(order.GetFixTag(FIX_TAG_PAIRID_PR));
	std::string strOrderId(order.GetHandle());
	gmPairIdMap.insert(pair<std::string,long>(strOrderId,nPairId));

	if (AddParamsToMap(order, gmPairMap) != SUCCESS)
	{
		_STRAT_LOG_VERB_(CERR << "Debug: Fail to Add New orders to Map - PairID ["<< nPairId
								<< "] Symbol [" <<order.GetSymbol()
								<< "] Size ["<< order.GetSize()
								<< "] Handle [" << order.GetHandle()
								<< "] FixTags [" <<order.GetFixTags()
								<< "]"
								<< std::endl);
		return FAILURE;
	}

	CINFO << "CLIENT NEW: "<<nPairId<<" "<<order.GetSymbol()<<" "<<order.GetSize()<<" "<< order.GetHandle()<<" "<<order.GetFixTags()<<std::endl;

	return SUCCESS;
}

int CalendarSpread::OnClientOrdValidRpl(CLIENT_ORDER& order)
{
    _STRAT_LOG_DBUG_(CDEBUG << "Debug: Validate Client RPL PairID [" << order.GetFixTag(FIX_TAG_PAIRID_PR) << "]" <<std::endl);

	return CalendarSpread::ValidateFIXTags(order);
}

int CalendarSpread::OnClientOrdRpl(CLIENT_ORDER& order)
{
        _STRAT_LOG_DBUG_(CDEBUG << "Start Client RPL - PairID [" << order.GetFixTag(FIX_TAG_PAIRID_PR)
        		<< "] Client OrderID [" << order.GetHandle()
        		<< "] Client Symbol [" << order.GetSymbol()
        		<< "]"
        		<<std::endl);

    AddParamsToMap(order,gmPairMapRpl);
	long nPairId = atol(order.GetFixTag(FIX_TAG_PAIRID_PR));
	PAIRMAPITER mIterRpl = gmPairMapRpl.find(nPairId);
	PAIRMAPITER mIter = gmPairMap.find(nPairId);

	if((mIter == gmPairMap.end()) ||(mIterRpl == gmPairMapRpl.end()))
	{
        _STRAT_LOG_VERB_(CWARN << "PairID [" << nPairId << "] not found in gmPairMap or gmPairMapRpl" << std::endl);
		return FAILURE;
	}

	PairParams *pPairParamsRpl = (PairParams*) mIterRpl->second;
	PairParams *pPairParams = (PairParams*) mIter->second;

	if(!pPairParams || !pPairParamsRpl)
	{
        _STRAT_LOG_VERB_(CWARN << "Invalid pointer pPairParams or pPairParamsRpl " << std::endl);

		return FAILURE;
	}

	// Stop the current working Pair Portfolio as client has sent a replace. Until replace is succcessful just stop this Pair.
	pPairParams->bPairRunning = false;

	// If the client strategy portfolio is ready and running, then start to cancel open orders.
	if (pPairParamsRpl->nEntryCount <= C_CALSPD_NUM_OF_LEGS_INDEX)
	{
		// Once it is ready means we got all the replaces for all the legs
		// Cancel existing street orders
		if (CancelBothStreetOrders(pPairParams) != POR_RET_SUCCESS)
		{
			 CERR << "PairID [" << nPairId
					 << "] RPL for Client OrderID [" << order.GetHandle()
					 << "] FAILED!!! -> CXL-REQ could not be sent out on all the active street orders"
					 << std::endl;
			return FAILURE;
		}

		_STRAT_LOG_DBUG_(PrintOrderMap(gmPairMap, nPairId));
		_STRAT_LOG_DBUG_(PrintOrderMapRpl(gmPairMapRpl));
	}

	std::string strOrderId(order.GetNewOrderId());
	gmPairIdMap.insert(pair<std::string, long>(strOrderId, nPairId));

	 CINFO << "PairID [" << nPairId
			 << "] RPL : Client OrderID [" << order.GetHandle()
			 << "] Symbol [" << order.GetSymbol()
			 << "] Size [" << order.GetSize()
			 << "] FixTags [" << order.GetFixTags()
			 << "]" << std::endl;

	return SUCCESS;
}

int CalendarSpread::OnClientOrdValidCancel(CLIENT_ORDER& order){return SUCCESS;}

int CalendarSpread::OnClientOrdCancel(CLIENT_ORDER& order)
{
	long nPairId = atol(order.GetFixTag(FIX_TAG_PAIRID_PR));
	CINFO << "CXL: "<<nPairId<<" "<<order.GetSymbol()<<" "<<order.GetSize()<<" "<< order.GetHandle()<<" "<<order.GetFixTags()<<std::endl;
	return SUCCESS;
}

int CalendarSpread::OnClientSendFills(CLIENT_EXEC& clientExec){return SUCCESS;}

int CalendarSpread::OnClientSendAck(CLIENT_EXEC& ClientExec)
{
	CLIENT_ORDER ClientOrder;
	ClientOrder = ClientExec.GetClientOrder();
	enOrderState eOrdState = ClientExec.GetReportStatus();

	if (eOrdState == STATE_REPLACED)
	{
		_STRAT_LOG_VERB_(CINFO << "Callback confirming RPLD state. Client OrderID [" << ClientExec.GetOrderID() << "]" << std::endl);

		_STRAT_LOG_DBUG_(PrintOrderMapRpl(gmPairMapRpl));

		long nPairId = atol(ClientOrder.GetFixTag(FIX_TAG_PAIRID_PR));
		PAIRMAPITER mIterRpl = gmPairMapRpl.find(nPairId);
		PAIRMAPITER mIter = gmPairMap.find(nPairId);

		if((mIter == gmPairMap.end()) ||(mIterRpl == gmPairMapRpl.end()))
		{
			if (mIter == gmPairMap.end())
			{
				_STRAT_LOG_DBUG_ (CDEBUG << "PairID [" << nPairId << "] not found in gmPairMap!" << std::endl);
			}
			if (mIterRpl == gmPairMapRpl.end())
			{
				_STRAT_LOG_DBUG_ (CDEBUG << "PairID [" << nPairId << "] not found in gmPairMapRpl!" << std::endl);
			}
			return FAILURE;
		}

		PairParams *pPairParamsRpl = (PairParams*) mIterRpl->second;
		PairParams *pPairParams = (PairParams*) mIter->second;

		if(!pPairParams || !pPairParamsRpl)
		{
	        _STRAT_LOG_DBUG_(CDEBUG << "Invalid pointer pPairParams or pPairParamsRpl " << std::endl);
			return FAILURE;
		}

	   _STRAT_LOG_VERB_ (CINFO << "RPLD : PairID [" << nPairId
								<< "] Client OrderID [" << ClientOrder.GetHandle()
								<< "] Client Symbol [" << ClientOrder.GetSymbol()
								<< "]" << std::endl);

		// Do the conditional processing ONLY after receving RPL Client Orders on BOTH the client orders
	    if(!strcmp(pPairParamsRpl->szSymbol1, ClientOrder.GetSymbol()))
	    {
	    	pPairParamsRpl->bCliRpld1 = true;
	    }
	    else if(!strcmp(pPairParamsRpl->szSymbol2, ClientOrder.GetSymbol()))
	    {
	    	pPairParamsRpl->bCliRpld2 = true;
	    }
	    else
	    {
	        _STRAT_LOG_VERB_ (CINFO << "~ Client ACK Debug ~: Invalid Client Symbol [" << ClientOrder.GetSymbol() << "]!" << std::endl);

			return FAILURE;
	    }

	    if (pPairParamsRpl-> bCliRpld1 == true && pPairParamsRpl-> bCliRpld2 == true)
		{
			//Update Maps
			gmPairMap.erase(mIter);
			delete pPairParams;

			pPairParamsRpl->bPairRunning=(bool)atoi(ClientOrder.GetFixTag(FIX_TAG_RUNNING_PR));

			gmPairMap.insert(pair<int, PairParams*>(nPairId, pPairParamsRpl));
			gmPairMapRpl.erase(mIterRpl);

	        _STRAT_LOG_VERB_(CINFO << "PairID [" << nPairId
	        		<< "] -> Committed the RPL for the PairParams map!"
	        		<< std::endl);

	        _STRAT_LOG_DBUG_(PrintOrderMap(gmPairMap, nPairId));
		}
	}
	else if (eOrdState == STATE_OPEN)
	{
	        _STRAT_LOG_DBUG_ (CDEBUG << "Callback confirming ACK state. Client OrderID [" << ClientExec.GetOrderID() << "]" << std::endl);
	}
	else if (eOrdState == STATE_PENDING_NEW)
	{
	        _STRAT_LOG_DBUG_ (CDEBUG << "Callback confirming PEND-NEW state. Client OrderID [" << ClientExec.GetOrderID() << "]" << std::endl);
	        ClientOrder.SendAck();
	}
	else
	{
	        _STRAT_LOG_DBUG_ (CDEBUG << "DEBUG: Callback in wrong state. Client OrderID [" << ClientExec.GetOrderID()
	        		<< "] -> State [" << StateToStr(eOrdState) << "]" << std::endl);
		return FAILURE;
	}
	return SUCCESS;
}

int CalendarSpread::OnClientSendOut(OMRULESEXPORT::CLIENT_EXEC&){return SUCCESS;}

int CalendarSpread::OnClientSendReject(OMRULESEXPORT::CLIENT_EXEC&){return SUCCESS;}

//Street Order callbacks
int CalendarSpread::OnStreetOrdNew(STREET_ORDER& streetOrder, CLIENT_ORDER& parentCLIENT_ORDER){return SUCCESS;}

int CalendarSpread::OnStreetOrdValidNew(STREET_ORDER& streetOrder, CLIENT_ORDER& parentOrder){return SUCCESS;}

int CalendarSpread::OnStreetOrdRpl(STREET_ORDER& streetOrder, CLIENT_ORDER& parentCLIENT_ORDER){return SUCCESS;}

int CalendarSpread::OnStreetOrdValidRpl(STREET_ORDER& streetOrder, CLIENT_ORDER& parentOrder){return SUCCESS;}

int CalendarSpread::OnStreetOrdCancel(STREET_ORDER& streetOrder, CLIENT_ORDER& parentCLIENT_ORDER){return SUCCESS;}

int CalendarSpread::OnStreetOrdValidCancel(STREET_ORDER& streetOrder, CLIENT_ORDER& parentOrder){return SUCCESS;}

int CalendarSpread::OnStreetOrdAck(STREET_ORDER& streetOrder){ return SUCCESS;}

int CalendarSpread::OnStreetExec(STREET_EXEC& StreetExec, CLIENT_ORDER& parentOrder){

	if(StreetExec.GetBrokerOrdId())
		{
		StreetExec.SetFixTag(37,StreetExec.GetBrokerOrdId());
		}
	if(StreetExec.GetExecId()){
		StreetExec.SetFixTag(17,StreetExec.GetExecId());
		}
	return SUCCESS;
}

int CalendarSpread::OnStreetOrdRej(STREET_ORDER& streetOrder, CLIENT_ORDER& parentCLIENT_ORDER){return SUCCESS;}

int CalendarSpread::OnStreetOrdOut(STREET_ORDER& so){return SUCCESS;}

int CalendarSpread::OnTimer(EVENT_DATA& eventData){return SUCCESS;}

int CalendarSpread::OnClientSendPendingRpl(OMRULESEXPORT::CLIENT_EXEC&){return SUCCESS;}

//Custom
int CalendarSpread::ValidateFIXTags(ORDER& order)
{

	std::string sFixDestination = order.GetFixTag(FIX_TAG_DESTINATION);
	if(sFixDestination.empty())
	{
		std::stringstream errMsg;
		errMsg << "Please set destination in tag " << FIX_TAG_DESTINATION;
		CERR << "Order Validation Failed, cannot send to destination " << sFixDestination
				<< " , rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}

	int nStreetOrderSize = atoi(order.GetFixTag(FIX_TAG_CLIPSIZE_PR));
	if(nStreetOrderSize<=0)
	{
		std::stringstream errMsg;
		errMsg << "INVClipSz " << FIX_TAG_CLIPSIZE_PR;
		CERR << "Order Validation Failed, invalid clip ord size, rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}
        
	int nLotSize = atoi(order.GetFixTag(FIX_TAG_LOTSIZE_PR));
	if(nLotSize<=0)
	{
		std::stringstream errMsg;
		errMsg << "INVLotSz " << FIX_TAG_CLIPSIZE_PR;
		CERR << "Order Validation Failed, invalid lot size, rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}

	double dRatio = atof(order.GetFixTag(FIX_TAG_RATIO_PR));
	if(dRatio < -dEpsilon) //Ratio is not mandatory tag zero is taken as one
	{
		std::stringstream errMsg;
		errMsg << "INVRatio " << FIX_TAG_RATIO_PR;
		CERR << "Order Validation Failed, invalid ratio, rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}

    std::string szOMUser=order.GetFixTag(FIX_TAG_OMUSER);
	if(szOMUser.length() <= 0 )
	{
		std::stringstream errMsg;
		errMsg << "INVOMUser " << FIX_TAG_OMUSER;
		CERR << "Order Validation Failed, invalid OM User, rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}
        
	std::string szPairSym=order.GetFixTag(FIX_TAG_PAIRSYM_PR);
	std::string szPairId=order.GetFixTag(FIX_TAG_PAIRID_PR);
	if(szPairSym.length() <= 0 || szPairId.length() <= 0)
	{
		std::stringstream errMsg;
		errMsg << "INVPairSym/ID " << FIX_TAG_PAIRSYM_PR << "&" << FIX_TAG_PAIRID_PR;
		CERR << "Order Validation Failed, invalid Pair side / ID, rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}

	long nPairId = atol(order.GetFixTag(FIX_TAG_PAIRID_PR));
	if(gmPairMap.count(nPairId) >= 2 )
	{
		std::stringstream errMsg;
                errMsg << "INVPairId " << FIX_TAG_RUNNING_PR;
                CERR << "Order Validation Failed, duplicate pair Id, rejecting client order!!! " << errMsg.str() << std::endl;
                order.SetRejectMsg(errMsg.str());
                return FAILURE;
	}
	int nPairRunning = atoi(order.GetFixTag(FIX_TAG_RUNNING_PR));
	if(!(nPairRunning==0 || nPairRunning ==1))
	{
		std::stringstream errMsg;
		errMsg << "INVRunning " << FIX_TAG_RUNNING_PR;
		CERR << "Order Validation Failed, invalid pair playpause tag [" << nPairRunning << "], rejecting client order!!! " << errMsg.str() << std::endl;
		order.SetRejectMsg(errMsg.str());
		return FAILURE;
	}

	return SUCCESS;
}

int CalendarSpread::AddParamsToMap(ORDER& order,PAIRMAP& mPairMap)
{
	//Populate Structure and add to map
	long nPairId = atol(order.GetFixTag(FIX_TAG_PAIRID_PR));
	int nLegId = atoi(order.GetFixTag(FIX_TAG_LEGID_PR));

	if(mPairMap.count(nPairId) <= 0)
	{
		//First Entry into pair
		_STRAT_LOG_DBUG_(CDEBUG << "Pair ID [" << nPairId
								<< "] ClientOrder ID [" << order.GetHandle()
								<< "] Leg ID [" << order.GetFixTag(FIX_TAG_LEGID_PR)
								<< "] : First Entry in mPairMap!" << std::endl);

		PairParams *pPairParams = new PairParams();
		if(!pPairParams)
			return FAILURE;

		pPairParams->nPairId = atol(order.GetFixTag(FIX_TAG_PAIRID_PR));
		if(nLegId == LEGID_1)
		{
			strcpy(pPairParams->szSymbol1,order.GetSymbol());
			strcpy(pPairParams->szClientOrdId1,order.GetHandle());
			strcpy(pPairParams->szPortfolio1,order.GetPortfolio());
			pPairParams->nClipSize1=atoi(order.GetFixTag(FIX_TAG_CLIPSIZE_PR));
			pPairParams->nLotSize1=atoi(order.GetFixTag(FIX_TAG_LOTSIZE_PR));
			if(atof(order.GetFixTag(FIX_TAG_RATIO_PR))>1) //Ratio is not mandatory tag zero is taken as one
				pPairParams->dRatio1=atof(order.GetFixTag(FIX_TAG_RATIO_PR));
			pPairParams->ensSide1=order.GetSide();
		}
		if(nLegId == LEGID_2)
		{
			strcpy(pPairParams->szSymbol2,order.GetSymbol());
			strcpy(pPairParams->szClientOrdId2,order.GetHandle());
			strcpy(pPairParams->szPortfolio2,order.GetPortfolio());
			pPairParams->nClipSize2=atoi(order.GetFixTag(FIX_TAG_CLIPSIZE_PR));
			pPairParams->nLotSize2=atoi(order.GetFixTag(FIX_TAG_LOTSIZE_PR));
			if(atof(order.GetFixTag(FIX_TAG_RATIO_PR))>1) //Ratio is not mandatory tag zero is taken as one
				pPairParams->dRatio2=atof(order.GetFixTag(FIX_TAG_RATIO_PR));
			pPairParams->ensSide2=order.GetSide();
		}
		strcpy(pPairParams->szOMUser,order.GetFixTag(FIX_TAG_OMUSER));
		strcpy(pPairParams->szDest,order.GetFixTag(FIX_TAG_DESTINATION));
		
		//Optimisation to save fix tag string construction on market data
		pPairParams->dBenchmark=atof(order.GetFixTag(FIX_TAG_BENCHMARK_PR));
		pPairParams->bPairRunning=(bool)atoi(order.GetFixTag(FIX_TAG_RUNNING_PR));

		pPairParams->nEntryCount++;

		mPairMap.insert(pair<int,PairParams*>(nPairId,pPairParams));

		_STRAT_LOG_DBUG_(PrintOrderMap(mPairMap, nPairId));

	}
	else
	{
		PAIRMAPITER mIter = mPairMap.find(nPairId);
		if(mIter == mPairMap.end())
			return FAILURE;

		_STRAT_LOG_DBUG_(CDEBUG << "Pair ID [" << nPairId
								<< "] ClientOrder ID [" << order.GetHandle()
								<< "] Leg ID [" << order.GetFixTag(FIX_TAG_LEGID_PR)
								<< "] : Second Entry in mPairMap!" << std::endl);

		PairParams *pPairParams = (PairParams*) mIter->second;

		if(!pPairParams)
			return FAILURE;
		
		if(nLegId == LEGID_1)
		{
			strcpy(pPairParams->szSymbol1,order.GetSymbol());
			strcpy(pPairParams->szClientOrdId1,order.GetHandle());
			pPairParams->nClipSize1=atoi(order.GetFixTag(FIX_TAG_CLIPSIZE_PR));
			pPairParams->nLotSize1=atoi(order.GetFixTag(FIX_TAG_LOTSIZE_PR));
			if(atof(order.GetFixTag(FIX_TAG_RATIO_PR))>1) //Ratio is not mandatory tag zero is taken as one
				pPairParams->dRatio1=atof(order.GetFixTag(FIX_TAG_RATIO_PR));
			pPairParams->ensSide1=order.GetSide();
		}
		if(nLegId == LEGID_2)
		{
			strcpy(pPairParams->szSymbol2,order.GetSymbol());
			strcpy(pPairParams->szClientOrdId2,order.GetHandle());
			pPairParams->nClipSize2=atoi(order.GetFixTag(FIX_TAG_CLIPSIZE_PR));
			pPairParams->nLotSize2=atoi(order.GetFixTag(FIX_TAG_LOTSIZE_PR));
			if(atof(order.GetFixTag(FIX_TAG_RATIO_PR))>1) //Ratio is not mandatory tag zero is taken as one
				pPairParams->dRatio2=atof(order.GetFixTag(FIX_TAG_RATIO_PR));
			pPairParams->ensSide2=order.GetSide();
		}
		pPairParams->bPairReady = true;

		_STRAT_LOG_DBUG_(PrintOrderMap(mPairMap, nPairId));
	}
	return SUCCESS;
}

int	CalendarSpread::GetClientOrder(CLIENT_ORDER& QuoteOrder, PairParams *pPairParams, bool bBuyLeg)
{
	if (bBuyLeg)
	{
		if(!GetClientOrderById(QuoteOrder, pPairParams->szClientOrdId1))
		{
			CERR << "PairID [" << pPairParams->nPairId
					<< "] : Failed to get buy client order by ID [ " << pPairParams->szClientOrdId1
					<< "]"
					<< std::endl;
			return FAILURE;
		}
	}
	else
	{
		if(!GetClientOrderById(QuoteOrder, pPairParams->szClientOrdId2))
		{
			CERR << "PairID [" << pPairParams->nPairId
					<< "] : Failed to get sell client order by ID [ " << pPairParams->szClientOrdId2
					<< "]"
					<< std::endl;
			return FAILURE;
		}
	}
	return SUCCESS;
}

int CalendarSpread::CancelUnfilledOrders(PairParams *pPairParams)
{
	CLIENT_ORDER BuyClientOrder, SellClientOrder;
	GetClientOrder(BuyClientOrder, pPairParams, true);
	GetClientOrder(SellClientOrder, pPairParams, false);

	if (BuyClientOrder.GetWorkingSize() >0 )
	{
		_STRAT_LOG_VERB_(CWARN << "PairID [" << pPairParams->nPairId
				<< "] Buy leg client order by ID [ " << BuyClientOrder.GetHandle()
				<< "] WorkingSize [" << BuyClientOrder.GetWorkingSize()
				<< "] : Cancelling all unfilled street orders!"
				<< std::endl);

		CancelAllOrdersForClientOrder(BuyClientOrder.GetHandle());
	}

	if (SellClientOrder.GetWorkingSize() >0 )
	{
		_STRAT_LOG_VERB_(CWARN << "PairID [" << pPairParams->nPairId
				<< "] Sell leg client order by ID [ " << SellClientOrder.GetHandle()
				<< "] WorkingSize [" << SellClientOrder.GetWorkingSize()
				<< "] : Cancelling all unfilled street orders!"
				<< std::endl);

		CancelAllOrdersForClientOrder(SellClientOrder.GetHandle());
	}

	return SUCCESS;
}


bool CalendarSpread::CalculateOrderSize(int iLeg1LotSize, int iLeg2LotSize, double dLeg1Ratio, double dLeg2Ratio, int iLeg1MktSize, int iLeg2MktSize, int iLeg1AvailQty, int iLeg2AvailQty, int& iLeg1OrderQty, int& iLeg2OrderQty)
{
	float iOrderSizeLeg1 = 0, iOrderSizeLeg2 = 0;
	if(iLeg1MktSize < iLeg1AvailQty)
		iLeg1AvailQty = iLeg1MktSize;
	if(iLeg2MktSize < iLeg2AvailQty)
		iLeg2AvailQty = iLeg2MktSize;
	float DesiredRatio = dLeg1Ratio/dLeg2Ratio;
	float iTempOrderSizeLeg1 = 0, iTempOrderSizeLeg2 = 0;
	if ( (iLeg1AvailQty/iLeg1LotSize) > (iLeg2AvailQty/iLeg2LotSize))
	{
		int MaxLotLeg2 = iLeg2AvailQty/iLeg2LotSize;
		int Check = 1;
		for(int i = MaxLotLeg2 ; i+1 > 0 ; i--)
		{
			if( Check ==1 )
			{
				iTempOrderSizeLeg2 = (i)*iLeg2LotSize ;
				iTempOrderSizeLeg1 = iTempOrderSizeLeg2*DesiredRatio ;

				float CheckParameter = 1;
				CheckParameter = iTempOrderSizeLeg1 - iLeg1LotSize*( ((int)iTempOrderSizeLeg1)/iLeg1LotSize );
				if ( CheckParameter == 0 && iTempOrderSizeLeg1 <= iLeg1AvailQty )
				{
					iOrderSizeLeg1 = iTempOrderSizeLeg1;
					iOrderSizeLeg2 = iTempOrderSizeLeg2;
					Check = 0;
				}
			}	
		}			
	}
	else 
	{
		int MaxLotLeg1 = iLeg1AvailQty/iLeg1LotSize;
		int Check = 1;
		for(int i = MaxLotLeg1 ; i+1 > 0 ; i--)
		{
			if ( Check == 1)
			{ 
				iTempOrderSizeLeg1 = (i)*iLeg1LotSize ;
				iTempOrderSizeLeg2 = iTempOrderSizeLeg1/DesiredRatio ;
				//cout<<iTempOrderSizeLeg1<<" "<<iTempOrderSizeLeg2<<endl;
				float CheckParameter = 1;
				CheckParameter = iTempOrderSizeLeg2 - iLeg1LotSize * ( ((int)iTempOrderSizeLeg2)/iLeg2LotSize);
				if ( CheckParameter == 0 && iTempOrderSizeLeg2 <= iLeg2AvailQty )
				{
					iOrderSizeLeg1 = iTempOrderSizeLeg1;
					iOrderSizeLeg2 = iTempOrderSizeLeg2;
					Check = 0;
				}
			}
		}
	}
	iLeg1OrderQty = (int)iOrderSizeLeg1;
	iLeg2OrderQty = (int)iOrderSizeLeg2;

	return (iLeg1OrderQty && iLeg2OrderQty);
}

/// UTILITY FUNCTION: Cancel all street orders for a given client order
int CalendarSpread::CancelAllOrdersForClientOrder(const char* pszClientOrderId)
{
    static STREET_ORDER_CONTAINER container;
    static STREET_ORDER TmpStreetOrder;
    if(!pszClientOrderId)
    	return FAILURE;

    CINFO << "Cancelling All Street orders for ClientOrderId [" << pszClientOrderId << "]" << std::endl;

    if(container.GetFirstActiveByClientOrderId(TmpStreetOrder, pszClientOrderId))
    {
    	do
    	{
    		if(!CanReplace(TmpStreetOrder))
    		{
    			CWARN << "Cannot Cancel Street Order [" << TmpStreetOrder.GetHandle()
    					<<"] , OrderState [" << StateToStr(TmpStreetOrder.GetOrderState())
    					<<"]"
    					<< std::endl;

    			continue;
    		}

    		TmpStreetOrder.SetClientOrdId(pszClientOrderId);

    		enErrorCode errStreetCancel = TmpStreetOrder.Cancel();
    		if(errStreetCancel != RULES_EO_OK)
    		{
    			CERR << "Cancel Fail for [" << TmpStreetOrder.GetHandle()
    					<<"] errStreetCancel [" << ErrorCodeToStr(errStreetCancel)
    					<<"]"
    					<< std::endl;
    		}
    		else
    		{
    			CINFO << "Cancel Success for [" << TmpStreetOrder.GetHandle() << "]" << std::endl;
    		}
    	}
    	while(container.GetNextActiveByClientOrderId(TmpStreetOrder));
    }
    return SUCCESS;
}

// UTILITY FUNCTION: Can we modify the existing street order?
bool CalendarSpread::CanReplace(STREET_ORDER& StreetOrder)
{
    if(StreetOrder.isReplacePending())
    {
    	_STRAT_LOG_VERB_(CWARN << "Street Order [" << StreetOrder.GetHandle()
    							<<"] Cannot Rpl/Cxl : Pending Replace! "
    							<< std::endl);
    	return false;
    }

    if(StreetOrder.isCancelPending())
	{
		_STRAT_LOG_VERB_(CWARN << "Street Order [" << StreetOrder.GetHandle()
								<<"] Cannot Rpl/Cxl : Pending Cancel! "
								<< std::endl);
		return false;
	}

    if(StreetOrder.GetOrderType() == TYPE_MARKET)
    {
		_STRAT_LOG_VERB_(CWARN << "Street Order [" << StreetOrder.GetHandle()
								<<"] Cannot Rpl: Market Order! "
								<< std::endl);
		return false;
    }

    if(StreetOrder.GetOrderTimeInForce() == TIF_IOC)
    {
    	_STRAT_LOG_VERB_(CWARN << "Street Order [" << StreetOrder.GetHandle()
    							<<"] Cannot Rpl: Market Order! "
    							<< std::endl);
    	return false;
    }

    switch(StreetOrder.GetOrderState())
    {
		case STATE_OPEN:
		case STATE_PARTIAL:
		case STATE_REPLACED:
			return true;
		case STATE_UNACKED:
		case STATE_FILLED:
		case STATE_CANCELLED:
		case STATE_PENDING_RPL:
		case STATE_REJECTED:
		case STATE_DONE:
		case STATE_INVALID:
		default:
			return false;
    }
    return false;
}

/// UTILITY FUNCTION: Return error code
const char* CalendarSpread::ErrorCodeToStr(enErrorCode e)
{
	static char *p[] = { "RULES_EO_OK",
						 "RULES_EO_FAIL",
						 "RULES_EO_NO_LVS",
						 "RULES_EO_NO_ORD",
						 "RULES_EO_NO_PORT",
						 "RULES_EO_OTHER_USER",
						 "RULES_EO_MAX_REJ",
						 "RULES_EO_NO_CLIENT_ORD",
						 "RULES_EO_NOT_ACTIVE",
						 "RULES_EO_REF_CXLD",
						 "RULES_EO_REF_RPLD",
						 "RULES_EO_INVALID_PRICE",
						 "RULES_EO_EXCEED_UNORD_SHRS",
						 "RULES_EO_DEST_NOT_UP",
						 "RULES_EO_SOCKET_FAIL" };
	switch (e)
	{
		case RULES_EO_OK:
			return p[0];
		case RULES_EO_FAIL:
			return p[1];
		case RULES_EO_NO_LVS:
			return p[2];
		case RULES_EO_NO_ORD:
			return p[3];
		case RULES_EO_NO_PORT:
			return p[4];
		case RULES_EO_OTHER_USER:
			return p[5];
		case RULES_EO_MAX_REJ:
			return p[6];
		case RULES_EO_NO_CLIENT_ORD:
			return p[7];
		case RULES_EO_NOT_ACTIVE:
			return p[8];
		case RULES_EO_REF_CXLD:
			return p[9];
		case RULES_EO_REF_RPLD:
			return p[10];
		case RULES_EO_INVALID_PRICE:
			return p[11];
		case RULES_EO_EXCEED_UNORD_SHRS:
			return p[12];
		case RULES_EO_DEST_NOT_UP:
			return p[13];
		case RULES_EO_SOCKET_FAIL:
			return p[14];
	}
	return p[1];
}

/// UTILITY FUNCTION: Return order state
const char* CalendarSpread::StateToStr(enOrderState e)
{
    static char *p[] = {"OPEN", "PART", "RPLD", "UNACKED", "FILLED", "CXLD", "PEND-RPL", "REJD", "DONE", "PEND-NEW", "INVALID"};
    switch(e)
    {
		case STATE_OPEN:
			return p[0];
		case STATE_PARTIAL:
			return p[1];
		case STATE_REPLACED:
			return p[2];
		case STATE_UNACKED:
			return p[3];
		case STATE_FILLED:
			return p[4];
		case STATE_CANCELLED:
			return p[5];
		case STATE_PENDING_RPL:
			return p[6];
		case STATE_REJECTED:
			return p[7];
		case STATE_DONE:
			return p[8];
		case STATE_PENDING_NEW:
			return p[9];
		case STATE_INVALID:
		default:
			return p[10];
    }
    return p[10];
}


//! @brief	Cancels the street orders for the pair
const CalendarSpread::E_POR_RET CalendarSpread::CancelBothStreetOrders(const PairParams *pPairParams)
{
	if (!pPairParams)
		return POR_RET_ERROR;

	STREET_ORDER_CONTAINER StreetOrdContainer;
	STREET_ORDER TmpStreetOrder;

	// Step 1: Cancel street orders under first client order
	for (bool b = StreetOrdContainer.GetFirstActiveByClientOrderId(TmpStreetOrder, pPairParams->szClientOrdId1); b;
	b = StreetOrdContainer.GetNextActiveByClientOrderId(TmpStreetOrder))
	{
		TmpStreetOrder.SetClientOrdId(pPairParams->szClientOrdId1);
		if (TmpStreetOrder.Cancel())
		{
			_STRAT_LOG_DBUG_(CDEBUG << "PairID [" << pPairParams->nPairId
									<< "] Sending cancel for Client OrderID [" << pPairParams->szClientOrdId1
									<< "] : Successfully sent CXL-REQ on Street Order ID [" << TmpStreetOrder.GetHandle()
									<< "]"
									<< std::endl);
		}
		else
		{
			_STRAT_LOG_VERB_(CERR << "PairID [" << pPairParams->nPairId
					<< "] Sending cancel for Client OrderID [" << pPairParams->szClientOrdId1
					<< "] : Failed to send CXL-REQ on Street Order ID [" << TmpStreetOrder.GetHandle()
					<< "]" << std::endl);
			return POR_RET_ERROR;
		}
	}

	// Step 2: Cancel street orders under second client order
	for (bool b = StreetOrdContainer.GetFirstActiveByClientOrderId(TmpStreetOrder, pPairParams->szClientOrdId2); b;
	b = StreetOrdContainer.GetNextActiveByClientOrderId(TmpStreetOrder))
	{
		TmpStreetOrder.SetClientOrdId(pPairParams->szClientOrdId2);
		if (TmpStreetOrder.Cancel())
		{
			_STRAT_LOG_DBUG_ (CDEBUG << "PairID [" << pPairParams->nPairId
					<< "] Sending cancel for Client OrderID [" << pPairParams->szClientOrdId2
					<< "] : Successfully sent CXL-REQ on Street Order ID [" << TmpStreetOrder.GetHandle()
					<< "]" << std::endl);
		}
		else
		{
			_STRAT_LOG_VERB_ (CERR << "PairID [" << pPairParams->nPairId
					<< "] Sending cancel for Client OrderID [" << pPairParams->szClientOrdId2
					<< "] : Failed to send CXL-REQ on Street Order ID [" << TmpStreetOrder.GetHandle()
					<< "]" << std::endl);
			return POR_RET_ERROR;
		}
	}

	return POR_RET_SUCCESS;
}


//! @brief	Prints Client Order Map.
// To be used in DEBUG Mode
void CalendarSpread::PrintOrderMap(PAIRMAP& mpPairMap, const long nPairId)
{
	CDEBUG << "Printing PairMap : PairID [" << nPairId << "]." << std::endl;
	if(nPairId < 1)
	{
		CDEBUG << "No PairID [" << nPairId
				<< "] passed; hence printing all." << std::endl;
	}
	if (mpPairMap.count(nPairId) <= 0)
	{
		CDEBUG << "PairID [" << nPairId
				<< "] is not found in the map!" << std::endl;
	}
	if(mpPairMap.count(nPairId))
	{
		PAIRMAPITER miIter =  mpPairMap.find(nPairId);
		{
			CDEBUG << "\n New Order"
			<< "| nPairId=" << miIter->second->nPairId
			<< "| bPairReady=" << miIter->second->bPairReady
			<< "| bPairRunning=" << miIter->second->bPairRunning
			<< "| szSymbol1=" << miIter->second->szSymbol1
			<< "| szSymbol2=" << miIter->second->szSymbol2
			<< "| szClientOrdId1=" << miIter->second->szClientOrdId1
			<< "| szClientOrdId2=" << miIter->second->szClientOrdId2
			<< "| szPortfolio1=" << miIter->second->szPortfolio1
			<< "| szPortfolio2=" << miIter->second->szPortfolio2
			<< "| dBenchmark=" << miIter->second->dBenchmark
			<< "| nClipSize1=" << miIter->second->nClipSize1
			<< "| nClipSize2=" << miIter->second->nClipSize2
			<< "| dRatio1=" << miIter->second->dRatio1
			<< "| dRatio2=" << miIter->second->dRatio2
			<< "| nLotSize1=" << miIter->second->nLotSize1
			<< "| nLotSize2=" << miIter->second->nLotSize2
			<< "| ensSide1=" << miIter->second->ensSide1
			<< "| ensSide2=" << miIter->second->ensSide2
			<< "| szOMUser=" << miIter->second->szOMUser
			<< "| szClientCode=" << miIter->second->szClientCode
			<< "| szCpCode=" << miIter->second->szCpCode
			<< "| szDest=" << miIter->second->szDest
			<< "\n"
			<< std::endl;
		}
	}
	else
	{
		PAIRMAPITER miIter;
		for(miIter = mpPairMap.begin(); miIter != mpPairMap.end() ; miIter++)
		{
			CDEBUG << "\n New Order"
			<< "| nPairId=" << miIter->second->nPairId
			<< "| bPairReady=" << miIter->second->bPairReady
			<< "| bPairRunning=" << miIter->second->bPairRunning
			<< "| szSymbol1=" << miIter->second->szSymbol1
			<< "| szSymbol2=" << miIter->second->szSymbol2
			<< "| szClientOrdId1=" << miIter->second->szClientOrdId1
			<< "| szClientOrdId2=" << miIter->second->szClientOrdId2
			<< "| szPortfolio1=" << miIter->second->szPortfolio1
			<< "| szPortfolio2=" << miIter->second->szPortfolio2
			<< "| dBenchmark=" << miIter->second->dBenchmark
			<< "| nClipSize1=" << miIter->second->nClipSize1
			<< "| nClipSize2=" << miIter->second->nClipSize2
			<< "| dRatio1=" << miIter->second->dRatio1
			<< "| dRatio2=" << miIter->second->dRatio2
			<< "| nLotSize1=" << miIter->second->nLotSize1
			<< "| nLotSize2=" << miIter->second->nLotSize2
			<< "| ensSide1=" << miIter->second->ensSide1
			<< "| ensSide2=" << miIter->second->ensSide2
			<< "| szOMUser=" << miIter->second->szOMUser
			<< "| szClientCode=" << miIter->second->szClientCode
			<< "| szCpCode=" << miIter->second->szCpCode
			<< "| szDest=" << miIter->second->szDest
			<< "\n"
			<< std::endl;
		}
	}
}

//! @brief	Prints Client Order Rpl Map.
// To be used in Debug Mode
void CalendarSpread::PrintOrderMapRpl(PAIRMAP& mpPairMap)
{
	CDEBUG << "PrintOrderMapRpl - printing all Rpl orders." << std::endl;
	PAIRMAPITER miIter;
	for(miIter = mpPairMap.begin(); miIter != mpPairMap.end() ; miIter++)
	{
		CDEBUG << "\n Rpl Order"
		<< "| nPairId=" << miIter->second->nPairId
		<< "| bPairReady=" << miIter->second->bPairReady
		<< "| bPairRunning=" << miIter->second->bPairRunning
		<< "| szSymbol1=" << miIter->second->szSymbol1
		<< "| szSymbol2=" << miIter->second->szSymbol2
		<< "| szClientOrdId1=" << miIter->second->szClientOrdId1
		<< "| szClientOrdId2=" << miIter->second->szClientOrdId2
		<< "| szPortfolio1=" << miIter->second->szPortfolio1
		<< "| szPortfolio2=" << miIter->second->szPortfolio2
		<< "| dBenchmark=" << miIter->second->dBenchmark
		<< "| nClipSize1=" << miIter->second->nClipSize1
		<< "| nClipSize2=" << miIter->second->nClipSize2
		<< "| dRatio1=" << miIter->second->dRatio1
		<< "| dRatio2=" << miIter->second->dRatio2
		<< "| nLotSize1=" << miIter->second->nLotSize1
		<< "| nLotSize2=" << miIter->second->nLotSize2
		<< "| ensSide1=" << miIter->second->ensSide1
		<< "| ensSide2=" << miIter->second->ensSide2
		<< "| szOMUser=" << miIter->second->szOMUser
		<< "| szClientCode=" << miIter->second->szClientCode
		<< "| szCpCode=" << miIter->second->szCpCode
		<< "| szDest=" << miIter->second->szDest
		<< "\n"
		<< std::endl;
	}
}
