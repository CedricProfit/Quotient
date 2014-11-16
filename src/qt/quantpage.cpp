#include "quantpage.h"
#include "ui_quantpage.h"

#include "guiutil.h"
#include "guiconstants.h"

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/foreach.hpp>
#include <map>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "net.h"
#include "db.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptValueIterator>

using namespace std;

qsreal trexLastPrice = 0;

CCriticalSection cs_markets;
map<qsreal, qsreal> mapBuys;
map<qsreal, qsreal> mapSells;
QVector<double> timeData(0), priceData(0), volumeData(0);
QVector<double> depthBuyPriceData(0), depthBuySumData(0);
QVector<double> depthSellPriceData(0), depthSellSumData(0);

QuantPage::QuantPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QuantPage)
{
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);
    updateMarketData();
    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateTimer_timeout()));
    updateTimer.setInterval(120000); // every 2 minutes
    updateTimer.start();
}

void QuantPage::updateTimer_timeout()
{
    if(fShutdown)
	return;

    updateMarketData();
}

void QuantPage::updateChart()
{
    if(fShutdown)
        return;

    ui->customPlot->clearPlottables();
    ui->customPlot->clearGraphs();
    ui->customPlot->clearItems();

    ui->volumePlot->clearPlottables();
    ui->volumePlot->clearGraphs();
    ui->volumePlot->clearItems();

    double binSize = 900; // bin data in 300 second (5 minute)// 900 second (15 minute) intervals
    // create candlestick chart:
    double startTime = timeData[0];
    QCPFinancial *candlesticks = new QCPFinancial(ui->customPlot->xAxis, ui->customPlot->yAxis);
    ui->customPlot->addPlottable(candlesticks);
    QCPFinancialDataMap data1 = QCPFinancial::timeSeriesToOhlc(timeData, priceData, binSize, startTime);
    candlesticks->setName("Price");
    candlesticks->setChartStyle(QCPFinancial::csCandlestick);
    candlesticks->setData(&data1, true);
    candlesticks->setWidth(binSize*0.9);
    candlesticks->setTwoColored(true);
    candlesticks->setBrushPositive(QColor(72, 191, 66));
    candlesticks->setBrushNegative(QColor(203, 19, 19));
    candlesticks->setPenPositive(QPen(QColor(72, 191, 66)));
    candlesticks->setPenNegative(QPen(QColor(203, 19, 19)));

    
    // create two bar plottables, for positive (green) and negative (red) volume bars:
    QCPBars *volumePos = new QCPBars(ui->volumePlot->xAxis, ui->volumePlot->yAxis);
    QCPBars *volumeNeg = new QCPBars(ui->volumePlot->xAxis, ui->volumePlot->yAxis);

    for (int i=0; i < timeData.count(); ++i)
    {
      double v = volumeData[i];
      (v < 0 ? volumeNeg : volumePos)->addData(timeData[i], qAbs(v)); // add data to either volumeNeg or volumePos, depending on sign of v
    }

    ui->customPlot->setAutoAddPlottableToLegend(false);
    ui->volumePlot->addPlottable(volumePos);
    ui->volumePlot->addPlottable(volumeNeg);
    volumePos->setWidth(binSize * 0.8);
    QPen pen;
    pen.setWidthF(1.2);
    pen.setColor(QColor(72, 191, 66));
    volumePos->setPen(pen);
    volumePos->setBrush(QColor(72, 191, 66, 20));
    volumeNeg->setWidth(binSize * 0.8);
    pen.setColor(QColor(201, 19, 19));
    volumeNeg->setPen(pen);
    volumeNeg->setBrush(QColor(203, 19, 19, 20));
  
    // configure axes of both main and bottom axis rect:
    ui->volumePlot->xAxis->setAutoTickStep(false);
    ui->volumePlot->xAxis->setTickStep(3600 * 24); // 24 hr tickstep
    ui->volumePlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    ui->volumePlot->xAxis->setDateTimeSpec(Qt::UTC);
    ui->volumePlot->xAxis->setDateTimeFormat("dd. MMM hh:mm");
    ui->volumePlot->xAxis->setTickLabelRotation(15);
    ui->volumePlot->xAxis->setTickLabelColor(QColor(137, 140, 146));
    ui->volumePlot->yAxis->setTickLabelColor(QColor(137, 140, 146));
    ui->volumePlot->yAxis->setAutoTickStep(false);
    ui->volumePlot->yAxis->setTickStep(3000);
    ui->volumePlot->rescaleAxes();
    ui->volumePlot->yAxis->grid()->setSubGridVisible(false);

    ui->customPlot->xAxis->setBasePen(Qt::NoPen);
    ui->customPlot->xAxis->setTickLabels(false);
    ui->customPlot->xAxis->setTicks(false); // only want vertical grid in main axis rect, so hide xAxis backbone, ticks, and labels
    ui->customPlot->xAxis->setAutoTickStep(false);
    ui->customPlot->xAxis->setTickStep(3600 * 24); // 6 hr tickstep
    ui->customPlot->rescaleAxes();
    //  ui->customPlot->xAxis->scaleRange(1.025, ui->customPlot->xAxis->range().center());
    ui->customPlot->yAxis->scaleRange(1.1, ui->customPlot->yAxis->range().center());
    ui->customPlot->xAxis->setTickLabelColor(QColor(137, 140, 146));
    ui->customPlot->yAxis->setTickLabelColor(QColor(137, 140, 146));
  
    // make axis rects' left side line up:
    QCPMarginGroup *group = new QCPMarginGroup(ui->customPlot);
    ui->customPlot->axisRect()->setMarginGroup(QCP::msLeft|QCP::msRight, group);

    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(10, 10, 10));
    plotGradient.setColorAt(1, QColor(0, 0, 0));
    ui->customPlot->setBackground(plotGradient);

    QLinearGradient volumePlotGradient;
    volumePlotGradient.setStart(0, 0);
    volumePlotGradient.setFinalStop(0, 150);
    volumePlotGradient.setColorAt(0, QColor(1, 1, 1));
    volumePlotGradient.setColorAt(1, QColor(0, 0, 0));
    ui->volumePlot->setBackground(volumePlotGradient);

    ui->customPlot->xAxis->grid()->setVisible(false);
    ui->customPlot->yAxis->grid()->setVisible(false);
    ui->customPlot->xAxis->grid()->setSubGridVisible(false);
    ui->customPlot->yAxis->grid()->setSubGridVisible(false);

    ui->customPlot->replot();
    ui->volumePlot->replot();
}

void QuantPage::updateDepthChart()
{
    if(fShutdown)
	return;

    ui->depthPlot->clearPlottables();
    ui->depthPlot->clearGraphs();
    ui->depthPlot->clearItems();
    ui->depthPlot->addGraph();
    ui->depthPlot->graph(0)->setPen(QPen(QColor(72, 191, 66))); // line color green for first graph
    ui->depthPlot->graph(0)->setBrush(QBrush(QColor(72, 191, 66, 20))); // first graph will be filled with translucent green
    ui->depthPlot->addGraph();
    ui->depthPlot->graph(1)->setPen(QPen(QColor(203, 19, 19))); // line color red for second graph
    ui->depthPlot->graph(1)->setBrush(QBrush(QColor(203, 19, 19, 20)));

    ui->depthPlot->graph(0)->setData(depthBuyPriceData, depthBuySumData);
    ui->depthPlot->graph(1)->setData(depthSellPriceData, depthSellSumData);
    ui->depthPlot->xAxis->setRangeLower(0);
    ui->depthPlot->xAxis->setRangeUpper(2 * depthBuyPriceData.last());
    double yr = depthSellSumData.last();
    if(depthBuySumData.last() > yr)
        yr = depthBuySumData.last();

    ui->depthPlot->yAxis->setRangeLower(0);
    ui->depthPlot->yAxis->setRangeUpper(yr);

    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(10, 10, 10));
    plotGradient.setColorAt(1, QColor(0, 0, 0));
    ui->depthPlot->setBackground(plotGradient);

    ui->depthPlot->xAxis->grid()->setVisible(false);
    ui->depthPlot->yAxis->grid()->setVisible(false);
    ui->depthPlot->xAxis->grid()->setSubGridVisible(false);
    ui->depthPlot->yAxis->grid()->setSubGridVisible(false);

    ui->depthPlot->xAxis->setTickLabelColor(QColor(137, 140, 146));
    ui->depthPlot->yAxis->setTickLabelColor(QColor(137, 140, 146));

    ui->depthPlot->replot();
}

void QuantPage::updateOrderBook()
{
    if(fShutdown)
        return;

    LOCK(cs_markets);
    depthBuyPriceData.clear();
    depthBuySumData.clear();
    depthSellPriceData.clear();
    depthSellSumData.clear();

    ui->askTableWidget->clearContents();
    ui->askTableWidget->setRowCount(0);
    int arow = 0;
    ui->bidTableWidget->clearContents();
    ui->bidTableWidget->setRowCount(0);
    int brow = 0;

    ui->askTableWidget->horizontalHeader()->hide();
    ui->bidTableWidget->horizontalHeader()->hide();
    ui->askTableWidget->verticalHeader()->hide();
    ui->bidTableWidget->verticalHeader()->hide();

    QFont fnt;
    fnt.setPointSize(8);
    fnt.setFamily("Monospace");

    map<double, double> sellDepthMap;

    BOOST_FOREACH(const PAIRTYPE(qsreal, qsreal)& sell, mapSells)
    {
	QString amount = QString::number(sell.second, 'f', 8);
	QString price = QString::number(sell.first, 'f', 8);
	if(sell.first > 0)
        {
	    // Add to depth map
	    if(sellDepthMap.size() == 0)
		sellDepthMap.insert(make_pair(COIN * sell.first, sell.second));
	    else
	    {
		// take the last amount and add to it, to produce running sum
		double prevSum = sellDepthMap.rbegin()->second;
		double newSum = prevSum + sell.second;
		sellDepthMap.insert(make_pair(COIN * sell.first, newSum));
	    }

	    // sell
            ui->askTableWidget->insertRow(0);
            QTableWidgetItem *newItemAM = new QTableWidgetItem(amount);
            QTableWidgetItem *newItemPR = new QTableWidgetItem(price);
            newItemAM->setFont(fnt);
    	    newItemPR->setFont(fnt);        
            ui->askTableWidget->setItem(0, 0, newItemAM);
            ui->askTableWidget->setItem(0, 1, newItemPR);
            arow++;	    
        }
    }

    map<double, double> buyDepthMap;
    // iterate buys in reverse
    BOOST_REVERSE_FOREACH(const PAIRTYPE(qsreal, qsreal)& buy, mapBuys)
    {
        if(buy.first > 0)
        {
            // Add to depth map
	    if(buyDepthMap.size() == 0)
		buyDepthMap.insert(make_pair(COIN * buy.first, buy.second));
	    else
	    {
		// take the first amount and add to it, to produce running sum
		double prevSum = buyDepthMap.begin()->second;
		double newSum = prevSum + buy.second;
		buyDepthMap.insert(make_pair(COIN * buy.first, newSum));
	    }
        }
    }

    BOOST_FOREACH(const PAIRTYPE(qsreal, qsreal)& buy, mapBuys)
    {
	QString amount = QString::number(buy.second, 'f', 8);
	QString price = QString::number(buy.first, 'f', 8);
	if(buy.first > 0)
        {
	    
            ui->bidTableWidget->insertRow(0);
            QTableWidgetItem *newItemAM = new QTableWidgetItem(amount);
            QTableWidgetItem *newItemPR = new QTableWidgetItem(price);
            newItemAM->setFont(fnt);
	    newItemPR->setFont(fnt);
            ui->bidTableWidget->setItem(0, 0, newItemAM);
            ui->bidTableWidget->setItem(0, 1, newItemPR);
            brow++;
        }
    }

    // add the sell and buy depth
    BOOST_FOREACH(const PAIRTYPE(double, double)& buy, buyDepthMap)
    {
	depthBuyPriceData.append(buy.first);
	depthBuySumData.append(buy.second);
    }

    BOOST_FOREACH(const PAIRTYPE(double, double)& sell, sellDepthMap)
    {
        depthSellPriceData.append(sell.first);
        depthSellSumData.append(sell.second);
    }

    int rowHeight = 12;
    ui->askTableWidget->verticalHeader()->setUpdatesEnabled(false); 
    for (unsigned int i = 0; i < ui->askTableWidget->rowCount(); i++)
        ui->askTableWidget->verticalHeader()->resizeSection(i, rowHeight);
    ui->askTableWidget->verticalHeader()->setUpdatesEnabled(true);

    ui->bidTableWidget->verticalHeader()->setUpdatesEnabled(false); 
    for (unsigned int i = 0; i < ui->bidTableWidget->rowCount(); i++)
        ui->bidTableWidget->verticalHeader()->resizeSection(i, rowHeight);
    ui->bidTableWidget->verticalHeader()->setUpdatesEnabled(true);

    ui->askTableWidget->scrollToBottom();
    ui->bidTableWidget->scrollToTop();

    updateDepthChart();
}

void QuantPage::updateMarketData()
{
    if(fShutdown)
	return;

    LOCK(cs_markets);
    mapSells.clear();
    mapBuys.clear();
    timeData.clear();
    priceData.clear();
    volumeData.clear();

    QUrl urlD("https://api.empoex.com/marketinfo/XQN-BTC");
    QNetworkRequest requestD;
    requestD.setUrl(urlD);
    QNetworkReply* currentReplyD = networkManager->get(requestD);
    connect(currentReplyD, SIGNAL(finished()), this, SLOT(eeMktDataReplyFinished()));

    // get order book from empo ex api
    QUrl url("https://api.empoex.com/orderbook/XQN-BTC");
    QNetworkRequest request;
    request.setUrl(url);
    QNetworkReply* currentReply = networkManager->get(request);
    connect(currentReply, SIGNAL(finished()), this, SLOT(eeHistoryReplyFinished()));

    //https://bittrex.com/api/v1.1/public/getmarketsummary?market=btc-ltc
    QUrl urlBD("https://bittrex.com/api/v1.1/public/getmarketsummary?market=btc-xqn");
    QNetworkRequest requestBD;
    requestBD.setUrl(urlBD);
    QNetworkReply* currentReplyBD = networkManager->get(requestBD);
    connect(currentReplyBD, SIGNAL(finished()), this, SLOT(trexMktDataReplyFinished()));

    //https://bittrex.com/api/v1.1/public/getorderbook?market=BTC-LTC&type=both&depth=50
    QUrl urlBO("https://bittrex.com/api/v1.1/public/getorderbook?market=BTC-XQN&type=both&depth=50");
    QNetworkRequest requestBO;
    requestBO.setUrl(urlBO);
    QNetworkReply* currentReplyBO = networkManager->get(requestBO);
    connect(currentReplyBO, SIGNAL(finished()), this, SLOT(trexHistoryReplyFinished()));

    // get market history for chart
    //https://bittrex.com/api/v1.1/public/getmarkethistory?market=BTC-XQN&count=50
    QUrl urlBH("https://bittrex.com/api/v1.1/public/getmarkethistory?market=BTC-XQN&count=50");
    QNetworkRequest requestBH;
    requestBH.setUrl(urlBH);
    QNetworkReply* currentReplyBH = networkManager->get(requestBH);
    connect(currentReplyBH, SIGNAL(finished()), this, SLOT(trexMktHistoryReplyFinished()));

   // get empox market history for chart
   // https://api.empoex.com/markethistory/XQN-BTC
    QUrl urlEH("https://api.empoex.com/markethistory/XQN-BTC");
    QNetworkRequest requestEH;
    requestEH.setUrl(urlEH);
    QNetworkReply* currentReplyEH = networkManager->get(requestEH);
    connect(currentReplyEH, SIGNAL(finished()), this, SLOT(eeMktHistoryReplyFinished()));

}

void QuantPage::trexMktDataReplyFinished()
{ 
    if(fShutdown)
	return;

    qDebug() << "trexMktDataReplyFinished()";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    
    // Now parse this JSON according to your needs !
    QScriptValue resultEntry = result.property("result");
    QScriptValueIterator it(resultEntry);

    LOCK(cs_markets);
    while(it.hasNext())
    {
        it.next();
        QScriptValue entry = it.value();

        qsreal last = entry.property("Last").toNumber(); 
        if(last > 0)
        {
            qsreal prevday = entry.property("PrevDay").toNumber();
            if(last < trexLastPrice)
            {
	        // red
                ui->trexLastPriceLabel->setObjectName("trexLastPriceLabel");
	        ui->trexLastPriceLabel->setStyleSheet("#trexLastPriceLabel { color: #FF0000; background-color:#000000; }");
   	        ui->trexLastPriceLabel->setText("B: " + QString::number(last, 'f', 8));
            }
            else if(last == trexLastPrice)
            {
	        // no change
            }
            else
            {
                ui->trexLastPriceLabel->setObjectName("trexLastPriceLabel");
   	        ui->trexLastPriceLabel->setStyleSheet("#trexLastPriceLabel { color: #00FF00; background-color:#000000; }");
	        ui->trexLastPriceLabel->setText("B: " + QString::number(last, 'f', 8));
            }

            trexLastPrice = last;
        }
    }
}

void QuantPage::eeMktHistoryReplyFinished()
{
    if(fShutdown)
	return;

    qDebug() << "eeMktHistoryReplyFinished()";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    QScriptValueIterator it(result.property("XQN-BTC"));
    LOCK(cs_markets);
    while(it.hasNext())
    {
        it.next();
        QScriptValue entry = it.value();
//{"XQN-BTC":[{"type":"Buy","date":1415845314,"amount":"142.80000000","price":"0.00002400","total":"0.00342720"},
        QString type = entry.property("type").toString();
	if(type != "")
	{
  	    QString rawDate = entry.property("date").toString();
	    qint32 intDate = entry.property("date").toInt32();
            QDateTime dt;
	    dt.setTime_t(intDate);
	    qsreal price = COIN * entry.property("price").toNumber();
	    qsreal vol = entry.property("amount").toNumber();

	    if(type == "Sell")
		vol = vol * -1;

	    timeData.append(dt.toTime_t());
	    priceData.append(price);
	    volumeData.append(vol);
        }
    }

    updateChart();
}

void QuantPage::trexMktHistoryReplyFinished()
{
    if(fShutdown)
	return;

    QString trexFormat = "yyyy-MM-ddThh:mm:ss.z";
    qDebug() << "trexMktHistoryReplyFinished()";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    QScriptValueIterator it(result.property("result"));
    LOCK(cs_markets);
    while(it.hasNext())
    {
        it.next();
        QScriptValue entry = it.value();
//[{"Id":664,"TimeStamp":"2014-11-14T02:38:08.307","Quantity":171.48666612,"Price":0.00002269,"Total":0.00389103,"FillType":"PARTIAL_FILL","OrderType":"SELL"},

        QString id = entry.property("Id").toString();
	if(id != "")
	{
  	    QString rawDate = entry.property("TimeStamp").toString();
	    if(!rawDate.contains("."))
	        rawDate = rawDate + ".0";

            QDateTime dt = QDateTime::fromString(rawDate, trexFormat);
	    dt.setTimeZone(QTimeZone(0));

	    qsreal price = COIN * entry.property("Price").toNumber();
	    qsreal vol = entry.property("Quantity").toNumber();

	    QString orderType = entry.property("OrderType").toString();
	    if(orderType == "SELL")
		vol = vol * -1;

	    timeData.append(dt.toTime_t());
	    priceData.append(price);
	    volumeData.append(vol);
        }
    }

    updateChart();
}

void QuantPage::trexHistoryReplyFinished()
{
    if(fShutdown)
	return;

    qDebug() << "trexHistoryReplyFinished()";
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    
    QScriptValue resultEntry = result.property("result");
    QScriptValueIterator itBuy(resultEntry.property("buy"));
    while(itBuy.hasNext())
    {
        itBuy.next();
        QScriptValue entry = itBuy.value();

        qsreal qty = entry.property("Quantity").toNumber();
        qsreal rate = entry.property("Rate").toNumber();

        if(mapBuys.find(rate) != mapBuys.end())
        {
	    mapBuys[rate] += qty;
        }
	else
	{
	    mapBuys.insert(make_pair(rate, qty));
	}

    }

    QScriptValueIterator itSell(resultEntry.property("sell"));
    while(itSell.hasNext())
    {
        itSell.next();
        QScriptValue entry = itSell.value();

        qsreal qty = entry.property("Quantity").toNumber();
        qsreal rate = entry.property("Rate").toNumber();

        if(mapSells.find(rate) != mapSells.end())
        {
	    mapSells[rate] += qty;
        }
	else
	{
	    mapSells.insert(make_pair(rate, qty));
	}
    }

    updateOrderBook();
}

void QuantPage::eeMktDataReplyFinished()
{ 
    if(fShutdown)
	return;

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    QScriptValueIterator it(result);

//[{"pairname":"XQN-BTC","last":"0.00002400","base_volume_24hr":"0.60486204","low":"0.00000989","high":"0.00002750","bid":"0.00001990",
//"ask":"0.00002400","open_buy_volume":"87,138.42828095","open_sell_volume":"5,642.11820899",
//"open_buy_volume_base":"0.25689740","open_sell_volume_base":"0.16165537","change":"+9.14%"}]
    while (it.hasNext()) {
        it.next();
        QScriptValue entry = it.value();
	QString pairname = entry.property("pairname").toString();

	if(pairname == "XQN-BTC")
	{
            QString change = entry.property("change").toString();
	    QString last = QString::number(entry.property("last").toNumber(), 'f', 8);
            QString volume = QString::number(entry.property("base_volume_24hr").toNumber(), 'f', 8);
            QString low = QString::number(entry.property("low").toNumber(), 'f', 8);
            QString high = QString::number(entry.property("high").toNumber(), 'f', 8);
            QString bid = QString::number(entry.property("bid").toNumber(), 'f', 8);
            QString ask = QString::number(entry.property("ask").toNumber(), 'f', 8);

	    if(change.startsWith("-"))
	    {
	        ui->lastPriceLabel->setObjectName("lastPriceLabel");
		ui->lastPriceLabel->setStyleSheet("#lastPriceLabel { color: #FF0000; background-color:#000000; }");
		ui->lastPriceLabel->setText("E: " + last);
	    }
	    else
	    {
	        ui->lastPriceLabel->setObjectName("lastPriceLabel");
		ui->lastPriceLabel->setStyleSheet("#lastPriceLabel { color: #00FF00; background-color:#000000; }");
		ui->lastPriceLabel->setText("E: " + last);
	    }

	}
    }

}

void QuantPage::eeHistoryReplyFinished()
{
    if(fShutdown)
	return;

    // get the market history, split into bids and asks, and update the tables and last price

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    qDebug() << "eeHistoryReplyFinished()";
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
   
    // {"XQN-BTC":[{"type":"Buy","date":1415845314,"amount":"142.80000000","price":"0.00002400","total":"0.00342720"},{"type":"Buy","date":1415845120,"amount":"202.98107194","price":"0.00002199","total":"0.00446355"},

    QScriptValue entries = result.property("XQN-BTC");

    LOCK(cs_markets);
    QScriptValue sellEntries = entries.property("sell");
    QScriptValueIterator itSell(sellEntries);
    while(itSell.hasNext()) {
	itSell.next();
	QScriptValue entry = itSell.value();
	qsreal qty = entry.property("amount").toNumber();
	qsreal rate = entry.property("price").toNumber();
	if(!QString::number(qty, 'f', 8).startsWith("0"))
	{
            if(mapSells.find(rate) != mapSells.end())
            {
	        mapSells[rate] += qty;
            }
	    else
	    {
	        mapSells.insert(make_pair(rate, qty));
	    }
        }
    }

    QScriptValue buyEntries = entries.property("buy");
    QScriptValueIterator itBuy(buyEntries);
    while(itBuy.hasNext()) {
	itBuy.next();
	QScriptValue entry = itBuy.value();
	qsreal qty = entry.property("amount").toNumber();
	qsreal rate = entry.property("price").toNumber();
	if(!QString::number(qty, 'f', 8).startsWith("0"))
	{
            if(mapBuys.find(rate) != mapBuys.end())
            {
	        mapBuys[rate] += qty;
            }
	    else
	    {
	        mapBuys.insert(make_pair(rate, qty));
	    }
        }        
    } 
    updateOrderBook();
}

QuantPage::~QuantPage()
{
    try
    {
        updateTimer.stop();
        delete networkManager;
        delete ui;
    }
    catch(std::exception& e)
    {
	// sometimes the QCustomPlot destructor throws
        qDebug() << e.what();
    }
}
