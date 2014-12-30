#include "qwirepage.h"
#include "ui_qwirepage.h"

#include "guiutil.h"
#include "guiconstants.h"
#include "qdex.h"
#include "ui_interface.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QNetworkProxyFactory>
#include <QWebSettings>
#include <QWebView>
#include <QPixmap>
#include <QImage>
#include <QNetworkReply>
#include <QDebug>
#include <QScrollArea>
#include <QSpacerItem>

#include "flowlayout.h"

extern std::map<uint64_t, CNewsFeedItem> mapNewsByTime;
extern std::map<uint64_t, CIndexFeedItem> mapIndexesByTime;
extern CCriticalSection cs_mapNews;
extern CCriticalSection cs_mapIndexes;
std::map<std::string, CNewsFeedItem> mapVideosById;

QWirePage::QWirePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QWirePage)
{
    ui->setupUi(this);
    ui->refreshButton->setVisible(false);

    this->setObjectName("qwirePage");
    this->setStyleSheet("#qwirePage { background-color: #202020; }");

    manager = new QNetworkAccessManager(this);
    loadFeed();

    subscribeToCoreSignals();
}

void QWirePage::on_refreshButton_clicked()
{
    loadFeed();
}

void QWirePage::loadFeed()
{
    loadIndexFeed();
    loadNewsFeed();
    loadVideoFeed();
}

void QWirePage::loadIndexFeed()
{
    remove(ui->verticalLayout_3);

    FlowLayout *flowLayout = new FlowLayout();

    std::set<std::string> setIdx;

    LOCK(cs_mapIndexes);
    BOOST_REVERSE_FOREACH(const PAIRTYPE(uint64_t, CIndexFeedItem)& p, mapIndexesByTime)
    {
        // show last 7 days of videos
	if(p.first >= (GetAdjustedTime() - (7 * 24 * 60 * 60)))
	{
	    p.second.CheckSignature();
            if(setIdx.find(p.second.strCode) == setIdx.end())
            {
   	        QWidget *idx = new QWidget();
	        idx->setMaximumSize(100, 100);
	        QVBoxLayout *idxLayout = new QVBoxLayout(idx);

                QLabel *codeLabel = new QLabel();
                codeLabel->setObjectName("codeLabel");
	        codeLabel->setStyleSheet("#codeLabel { color: #ffffff; }");
                codeLabel->setText(QString::fromStdString(p.second.strCode));
                idxLayout->addWidget(codeLabel);

                QWidget *ch = new QWidget();
                ch->setMaximumSize(100, 60);
                idxLayout->addWidget(ch);

                QLabel *amtLabel = new QLabel();
                amtLabel->setText(QString::number((p.second.nIndexValue / COIN), 'f', 2));
	        idxLayout->addWidget(amtLabel);

	        flowLayout->addWidget(idx);
            }
        }
    }

    ui->verticalLayout_3->addLayout(flowLayout);
    QSpacerItem *spacer = new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding);
    ui->verticalLayout_3->addSpacerItem(spacer);
}

void QWirePage::loadNewsFeed()
{
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(0);
    int row = 0;

    LOCK(cs_mapNews);
    BOOST_REVERSE_FOREACH(const PAIRTYPE(uint64_t, CNewsFeedItem)& p, mapNewsByTime)
    {
	// show last 7 days of videos
	if(p.first >= (GetAdjustedTime() - (7 * 24 * 60 * 60)))
	{
	    p.second.CheckSignature();
	    if(!p.second.bIsVideo)
	    {
		// Add the news item to the table
		ui->tableWidget->insertRow(row);
		QLabel *newsItemLabel = new QLabel();
		newsItemLabel->setText(QString::fromStdString("<a href=\"" + p.second.strLink + "\">" + p.second.strTitle + "</a>"));
		newsItemLabel->setToolTip(QString::fromStdString(p.second.strSummary));
		newsItemLabel->setOpenExternalLinks(true);
		newsItemLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

                QTableWidgetItem *newItemSR = new QTableWidgetItem(QString::fromStdString(p.second.strSource));
                QTableWidgetItem *newItemDT = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat(p.second.nTimestamp)));

		ui->tableWidget->setItem(row, 0, newItemDT);
	        ui->tableWidget->setItem(row, 1, newItemSR);
	        ui->tableWidget->setCellWidget(row, 2, newsItemLabel);

		row++;
            }
        }
    }
}

void QWirePage::loadVideoFeed()
{
    remove(ui->verticalLayout_2);
    QLabel *titleLabel = new QLabel(this);
    titleLabel->setText("Video Feed");
    titleLabel->setObjectName("titleLabel");
    titleLabel->setStyleSheet("#titleLabel { color:#4CFF00;font-weight:bold;font-size:16pt; }");
    ui->verticalLayout_2->addWidget(titleLabel);

    QWidget *central = new QWidget();
    central->setMaximumWidth(270);
    central->setObjectName("central");
    central->setStyleSheet("#central { background-color: #202020; }");

    QScrollArea *scroll = new QScrollArea();
    scroll->setMaximumWidth(300);
    scroll->setObjectName("scroll");
    scroll->setStyleSheet("#scroll { background-color: #202020; }");
    videos = new QVBoxLayout(central);
    scroll->setWidget(central);
    scroll->setWidgetResizable(true);
    ui->verticalLayout_2->addWidget(scroll);
    
    LOCK(cs_mapNews);
    BOOST_REVERSE_FOREACH(const PAIRTYPE(uint64_t, CNewsFeedItem)& p, mapNewsByTime)
    {
	// show last 7 days of videos
	if(p.first >= (GetAdjustedTime() - (7 * 24 * 60 * 60)))
	{
	    p.second.CheckSignature();
	    if(p.second.bIsVideo)
	    {
 	        // send a request for the thumbnail image for each video
	        std::string vidUrl = "http://img.youtube.com/vi/" + p.second.strVideoCode + "/sddefault.jpg";
                printf("Requesting video thumbnail %s", vidUrl.c_str());
	        
	        //qDebug() << vidUrl.c_str();
	        // put the news item in a map by video id
	        mapVideosById.insert(make_pair(vidUrl, p.second));
	        QUrl url(QString::fromStdString(vidUrl));
                QNetworkRequest request;
                request.setUrl(url);
                QNetworkReply* currentReply = manager->get(request);
                connect(currentReply, SIGNAL(finished()), this, SLOT(getImgReplyFinished()));
	    }
        }
    }

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    videos->addWidget(spacer);
}

void QWirePage::remove(QLayout* layout)
{
    QLayoutItem* child;
    while(layout->count()!=0)
    {
        child = layout->takeAt(0);
        if(child->layout() != 0)
        {
            remove(child->layout());
        }
        else if(child->widget() != 0)
        {
            delete child->widget();
        }

        delete child;
    }
}

void QWirePage::getImgReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray bytes(reply->readAll());

    QImage image = QImage::fromData(bytes, "JPG");

    QLabel *label = new QLabel(this);
    label->setPixmap(QPixmap::fromImage(image));
    label->setFixedWidth(260);
    label->setFixedHeight(157);
    label->setScaledContents(true);
    //label->setOpenExternalLinks(true);
    //label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    //label->setText("<a href=\"http://www.google.com\">View Video</a>");
    videos->addWidget(label);

    // get the video item
    if(mapVideosById.find(reply->url().toString().toStdString()) != mapVideosById.end())
    {
        CNewsFeedItem p = mapVideosById[reply->url().toString().toStdString()];
        QLabel *linkLabel = new QLabel(this);
        linkLabel->setFixedWidth(260);
        linkLabel->setOpenExternalLinks(true);
        linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        linkLabel->setToolTip(QString::fromStdString(p.strSummary));
        linkLabel->setText(QString::fromStdString("<a href=\"" + p.strLink + "\">" + p.strTitle + "</a>"));
        videos->addWidget(linkLabel);
    }
}

void QWirePage::updateFeed(const QString &hash, int status)
{
    loadFeed();
}

static void NotifyIndexFeedChanged(QWirePage *qwirePage, const uint256 &hash, ChangeType status)
{
    OutputDebugStringF("NotifyIndexItemChanged %s status=%i\n", hash.GetHex().c_str(), status);
    QMetaObject::invokeMethod(qwirePage, "updateFeed", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(hash.GetHex())),
        Q_ARG(int, status));
}

static void NotifyNewsFeedChanged(QWirePage *qwirePage, const uint256 &hash, ChangeType status)
{
    OutputDebugStringF("NotifyNewsItemChanged %s status=%i\n", hash.GetHex().c_str(), status);
    QMetaObject::invokeMethod(qwirePage, "updateFeed", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(hash.GetHex())),
        Q_ARG(int, status));
}

void QWirePage::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.NotifyIndexFeedChanged.connect(boost::bind(NotifyIndexFeedChanged, this, _1, _2));
    uiInterface.NotifyNewsFeedChanged.connect(boost::bind(NotifyNewsFeedChanged, this, _1, _2));
}

void QWirePage::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.NotifyIndexFeedChanged.disconnect(boost::bind(NotifyIndexFeedChanged, this, _1, _2));
    uiInterface.NotifyNewsFeedChanged.disconnect(boost::bind(NotifyNewsFeedChanged, this, _1, _2));
}

QWirePage::~QWirePage()
{
    unsubscribeFromCoreSignals();
    delete ui;
}
