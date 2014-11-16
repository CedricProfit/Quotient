#include "profitexplorerpage.h"
#include "ui_profitexplorerpage.h"

#include "guiutil.h"
#include "guiconstants.h"

#include "init.h"
#include "main.h"
#include "wallet.h"
#include "bitcoinrpc.h"

#include <QAbstractItemDelegate>
#include <QPainter>

QVector<double> nTimeData(0), myStakeData(0), netStakeData(0), difficultyData(0);

ProfitExplorerPage::ProfitExplorerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProfitExplorerPage)
{
    ui->setupUi(this);
    loadStakeChart(true);

    connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateTimer_timeout()));
    updateTimer.setInterval(123000); // every 123 seconds (approx 2 blocks)
    updateTimer.start();
}

void ProfitExplorerPage::updateTimer_timeout()
{
    if(fShutdown)
	return;

    if(ui->checkBox->isChecked())
        loadStakeChart(false);
}

void ProfitExplorerPage::on_recomputeButton_clicked()
{
    loadStakeChart(false);
}

void ProfitExplorerPage::loadStakeChart(bool firstRun)
{
        if(fShutdown)
	return;

    nTimeData.clear();
    netStakeData.clear();
    myStakeData.clear();
    difficultyData.clear();

    // go back this many blocks max
    int max = ui->spinBox->value();
    int i = 0;

    BOOST_REVERSE_FOREACH(const PAIRTYPE(uint256, CBlockIndex*)& b, mapBlockIndex)
    {
        if(i >= max)
            break;

        CBlockIndex* pindex = b.second;
        if(pindex->IsProofOfStake())
	{
            nTimeData.append(pindex->nTime);
	    netStakeData.append(pindex->nMint / COIN);

	    // Read the block in and check if the coinstake is ours
	    CBlock block;
	    block.ReadFromDisk(pindex, true);
	    if(block.IsProofOfStake()) // this should always be true here
	    {
		difficultyData.append(GetDifficulty(pindex));
		if(pwalletMain->IsMine(block.vtx[1]))
		{
		    myStakeData.append(pindex->nMint / COIN);
		}
		else
		{
		    myStakeData.append(0);
		}
	    }
	    else
	    {
		myStakeData.append(0); // should never happen
	    }
	}
        ++i;
    }    

    if(!firstRun)
    {
        uint64_t nMinWeight = 0, nMaxWeight = 0, nWeight = 0;
        pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);

        uint64_t nNetworkWeight = 0;
        if(pindexBest)
            nNetworkWeight = GetPoSKernelPS();
        bool staking = nLastCoinStakeSearchInterval && nWeight;
        int nExpectedTime = staking ? (nTargetSpacing * nNetworkWeight / nWeight) : -1;

        ui->stakingLabel->setText(staking ? "Enabled" : "Disabled");
        if(pindexBest)
            ui->difficultyLabel->setText(QString::number(GetDifficulty(GetLastBlockIndex(pindexBest, true))));
        ui->weightLabel->setText(QString::number(nWeight));
        ui->netWeightLabel->setText(QString::number(nNetworkWeight));
        ui->timeToStakeLabel->setText(QString::number(nExpectedTime) + " secs");
    }

    //qDebug() << "Stake blocks processed:";
    //qDebug() << i;
    ui->customPlot->clearPlottables();
    ui->customPlot->clearGraphs();
    ui->customPlot->clearItems();
    ui->customPlot->addGraph();
    ui->customPlot->graph(0)->setPen(QPen(QColor(206, 206, 206))); // line color green for first graph
    ui->customPlot->graph(0)->setBrush(QBrush(QColor(206, 206, 206, 20))); // first graph will be filled with translucent green
    ui->customPlot->addGraph();
    ui->customPlot->graph(1)->setPen(QPen(QColor(76, 255, 0))); // line color red for second graph
    ui->customPlot->graph(1)->setBrush(QBrush(QColor(76, 255, 0, 20)));

    if(ui->networkCheckBox->isChecked())
        ui->customPlot->graph(0)->setData(nTimeData, netStakeData);
    ui->customPlot->graph(1)->setData(nTimeData, myStakeData);
    ui->customPlot->xAxis->setRangeLower(nTimeData.first());
    ui->customPlot->xAxis->setRangeUpper(nTimeData.last());

    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(10, 10, 10));
    plotGradient.setColorAt(1, QColor(0, 0, 0));
    ui->customPlot->setBackground(plotGradient);

    ui->customPlot->xAxis->grid()->setVisible(false);
    ui->customPlot->yAxis->grid()->setVisible(false);
    ui->customPlot->xAxis->grid()->setSubGridVisible(false);
    ui->customPlot->yAxis->grid()->setSubGridVisible(false);

    ui->customPlot->xAxis->setAutoTickStep(false);
    ui->customPlot->xAxis->setTickStep(3600 * 24); // 24 hr tickstep
    ui->customPlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    ui->customPlot->xAxis->setDateTimeSpec(Qt::UTC);
    ui->customPlot->xAxis->setDateTimeFormat("dd. MMM hh:mm");
    ui->customPlot->xAxis->setTickLabelRotation(15);

    ui->customPlot->xAxis->setTickLabelColor(QColor(137, 140, 146));
    ui->customPlot->yAxis->setTickLabelColor(QColor(137, 140, 146));

    ui->customPlot->rescaleAxes();

    ui->customPlot->xAxis->setLabelColor(QColor(137, 140, 146));
    ui->customPlot->yAxis->setLabelColor(QColor(137, 140, 146));
    ui->customPlot->yAxis->setLabel("$XQN Minted");
    ui->customPlot->xAxis->setLabel("Stake Block Generation Time");

    ui->customPlot->replot();


    ui->difficultyPlot->clearPlottables();
    ui->difficultyPlot->clearGraphs();
    ui->difficultyPlot->clearItems();
    ui->difficultyPlot->addGraph();
    ui->difficultyPlot->graph(0)->setPen(QPen(QColor(76, 255, 0))); // line color green for first graph
    ui->difficultyPlot->graph(0)->setBrush(QBrush(QColor(76, 255, 0, 20))); // first graph will be filled with translucent green
    
    ui->difficultyPlot->graph(0)->setData(nTimeData, difficultyData);
    ui->difficultyPlot->xAxis->setRangeLower(nTimeData.first());
    ui->difficultyPlot->xAxis->setRangeUpper(nTimeData.last());

    QLinearGradient diffPlotGradient;
    diffPlotGradient.setStart(0, 0);
    diffPlotGradient.setFinalStop(0, 350);
    diffPlotGradient.setColorAt(0, QColor(10, 10, 10));
    diffPlotGradient.setColorAt(1, QColor(0, 0, 0));
    ui->difficultyPlot->setBackground(diffPlotGradient);

    ui->difficultyPlot->xAxis->grid()->setVisible(false);
    ui->difficultyPlot->yAxis->grid()->setVisible(false);
    ui->difficultyPlot->xAxis->grid()->setSubGridVisible(false);
    ui->difficultyPlot->yAxis->grid()->setSubGridVisible(false);

    ui->difficultyPlot->xAxis->setAutoTickStep(false);
    ui->difficultyPlot->xAxis->setTickStep(3600 * 24); // 24 hr tickstep
    ui->difficultyPlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    ui->difficultyPlot->xAxis->setDateTimeSpec(Qt::UTC);
    ui->difficultyPlot->xAxis->setDateTimeFormat("dd. MMM hh:mm");
    ui->difficultyPlot->xAxis->setTickLabelRotation(15);

    ui->difficultyPlot->xAxis->setTickLabelColor(QColor(137, 140, 146));
    ui->difficultyPlot->yAxis->setTickLabelColor(QColor(137, 140, 146));

    ui->difficultyPlot->rescaleAxes();
    ui->difficultyPlot->yAxis->setTickStep(0.00005);
    ui->difficultyPlot->xAxis->setLabelColor(QColor(137, 140, 146));
    ui->difficultyPlot->yAxis->setLabelColor(QColor(137, 140, 146));
    ui->difficultyPlot->yAxis->setLabel("Difficulty");
    ui->difficultyPlot->xAxis->setTickLabels(false);
    //ui->difficultyPlot->xAxis->setLabel("Stake Block Generation Time");

    ui->difficultyPlot->replot();

}

ProfitExplorerPage::~ProfitExplorerPage()
{
    delete ui;
}
