#include "consolepage.h"
#include "ui_consolepage.h"
#include "rpcconsole.h"

#include "clientmodel.h"
#include "bitcoinrpc.h"
#include "guiutil.h"

#include <QTime>
#include <QTimer>
#include <QThread>
#include <QTextEdit>
#include <QKeyEvent>
#include <QUrl>
#include <QScrollBar>

#include <openssl/crypto.h>

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValue>
#include <QtScript/QScriptValueIterator>

const int CONSOLE_SCROLLBACK = 50;
const int CONSOLE_HISTORY = 50;

const QSize ICON_SIZE(24, 24);

const struct {
    const char *url;
    const char *source;
} ICON_MAPPING[] = {
    {"cmd-request", ":/icons/tx_input"},
    {"cmd-reply", ":/icons/tx_output"},
    {"cmd-error", ":/icons/tx_output"},
    {"misc", ":/icons/tx_inout"},
    {NULL, NULL}
};


ConsolePage::ConsolePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConsolePage),
    historyPtr(0)
{
    ui->setupUi(this);

    // Install event filter for up and down arrow
    ui->lineEdit->installEventFilter(this);
    ui->messagesWidget->installEventFilter(this);
    ui->clearButton->setVisible(false);

    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    startExecutor();
    clear();

    //connect(&networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(onResult(QNetworkReply*)));
}

ConsolePage::~ConsolePage()
{
    emit stopExecutor();
    delete ui;
}

bool ConsolePage::eventFilter(QObject* obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress) // Special key handling
    {
        QKeyEvent *keyevt = static_cast<QKeyEvent*>(event);
        int key = keyevt->key();
        Qt::KeyboardModifiers mod = keyevt->modifiers();
        switch(key)
        {
        case Qt::Key_Up: if(obj == ui->lineEdit) { browseHistory(-1); return true; } break;
        case Qt::Key_Down: if(obj == ui->lineEdit) { browseHistory(1); return true; } break;
        case Qt::Key_PageUp: /* pass paging keys to messages widget */
        case Qt::Key_PageDown:
            if(obj == ui->lineEdit)
            {
                QApplication::postEvent(ui->messagesWidget, new QKeyEvent(*keyevt));
                return true;
            }
            break;
        default:
            // Typing in messages widget brings focus to line edit, and redirects key there
            // Exclude most combinations and keys that emit no text, except paste shortcuts
            if(obj == ui->messagesWidget && (
                  (!mod && !keyevt->text().isEmpty() && key != Qt::Key_Tab) ||
                  ((mod & Qt::ControlModifier) && key == Qt::Key_V) ||
                  ((mod & Qt::ShiftModifier) && key == Qt::Key_Insert)))
            {
                ui->lineEdit->setFocus();
                QApplication::postEvent(ui->lineEdit, new QKeyEvent(*keyevt));
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ConsolePage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
    }
}

static QString categoryClass(int category)
{
    switch(category)
    {
    case ConsolePage::CMD_REQUEST:  return "cmd-request"; break;
    case ConsolePage::CMD_REPLY:    return "cmd-reply"; break;
    case ConsolePage::CMD_ERROR:    return "cmd-error"; break;
    default:                       return "misc";
    }
}

void ConsolePage::entryFocus()
{
    ui->lineEdit->setFocus();
}

void ConsolePage::clear()
{
    ui->messagesWidget->clear();
    ui->lineEdit->clear();
    ui->lineEdit->setFocus();

    // Add smoothly scaled icon images.
    // (when using width/height on an img, Qt uses nearest instead of linear interpolation)
    for(int i=0; ICON_MAPPING[i].url; ++i)
    {
        ui->messagesWidget->document()->addResource(
                    QTextDocument::ImageResource,
                    QUrl(ICON_MAPPING[i].url),
                    QImage(ICON_MAPPING[i].source).scaled(ICON_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }

    // Set default style sheet
    ui->messagesWidget->document()->setDefaultStyleSheet(
                "table { }"
                "td.time { color: #eeeeee; padding-top: 3px; } "
                "td.message { font-family: Monospace; font-size: 12px; } "
                "td.cmd-request { color: #00bb00; } "
                "td.cmd-error { color: #ff6600; } "
                "b { color: #cccccc; } "
                );

    message(CMD_REPLY, (tr("Welcome to the Quotient RPC console.") + "<br>" +
                        tr("Use up and down arrows to navigate history, and <b>Ctrl-L</b> or clear to clear screen.") + "<br>" +
                        tr("Type <b>help</b> for an overview of available commands.")), true);
}

void ConsolePage::message(int category, const QString &message, bool html)
{
    QTime time = QTime::currentTime();
    QString timeString = time.toString();
    QString out;
    out += "<table><tr><td class=\"time\" width=\"65\">" + timeString + "</td>";
    out += "<td class=\"icon\" width=\"32\"><img src=\"" + categoryClass(category) + "\"></td>";
    out += "<td class=\"message " + categoryClass(category) + "\" valign=\"middle\">";
    if(html)
        out += message;
    else
        out += GUIUtil::HtmlEscape(message, true);
    out += "</td></tr></table>";
    ui->messagesWidget->append(out);
}

void ConsolePage::on_lineEdit_returnPressed()
{
    QString cmd = ui->lineEdit->text();
    ui->lineEdit->clear();

    if(!cmd.isEmpty())
    {
        if(cmd.toStdString() == "clear")
	{
	    message(CMD_REQUEST, cmd);
	    clear();
	    // Remove command, if already in history
            history.removeOne(cmd);
            // Append command to history
            history.append(cmd);
            // Enforce maximum history size
            while(history.size() > CONSOLE_HISTORY)
                history.removeFirst();
            // Set pointer to end of history
            historyPtr = history.size();
            // Scroll console view to end
            scrollToEnd();
	}
	else if(cmd.toStdString() == "markethistory")
	{
	    message(CMD_REQUEST, cmd);
	    dumpmarkethistory();
	    // Remove command, if already in history
            history.removeOne(cmd);
            // Append command to history
            history.append(cmd);
            // Enforce maximum history size
            while(history.size() > CONSOLE_HISTORY)
                history.removeFirst();
            // Set pointer to end of history
            historyPtr = history.size();
            // Scroll console view to end
            scrollToEnd();
	}
	else if(cmd.toStdString() == "marketdata")
	{
	    message(CMD_REQUEST, cmd);
	    marketdata();
	    // Remove command, if already in history
            history.removeOne(cmd);
            // Append command to history
            history.append(cmd);
            // Enforce maximum history size
            while(history.size() > CONSOLE_HISTORY)
                history.removeFirst();
            // Set pointer to end of history
            historyPtr = history.size();
            // Scroll console view to end
            scrollToEnd();
	}
	else
	{
            message(CMD_REQUEST, cmd);
            emit cmdRequest(cmd);
            // Remove command, if already in history
            history.removeOne(cmd);
            // Append command to history
            history.append(cmd);
            // Enforce maximum history size
            while(history.size() > CONSOLE_HISTORY)
                history.removeFirst();
            // Set pointer to end of history
            historyPtr = history.size();
            // Scroll console view to end
            scrollToEnd();
	}
    }
}

void ConsolePage::browseHistory(int offset)
{
    historyPtr += offset;
    if(historyPtr < 0)
        historyPtr = 0;
    if(historyPtr > history.size())
        historyPtr = history.size();
    QString cmd;
    if(historyPtr < history.size())
        cmd = history.at(historyPtr);
    ui->lineEdit->setText(cmd);
}

void ConsolePage::startExecutor()
{
    QThread* thread = new QThread;
    RPCExecutor *executor = new RPCExecutor();
    executor->moveToThread(thread);

    // Notify executor when thread started (in executor thread)
    connect(thread, SIGNAL(started()), executor, SLOT(start()));
    // Replies from executor object must go to this object
    connect(executor, SIGNAL(reply(int,QString)), this, SLOT(message(int,QString)));
    // Requests from this object must go to executor
    connect(this, SIGNAL(cmdRequest(QString)), executor, SLOT(request(QString)));
    // On stopExecutor signal
    // - queue executor for deletion (in execution thread)
    // - quit the Qt event loop in the execution thread
    connect(this, SIGNAL(stopExecutor()), executor, SLOT(deleteLater()));
    connect(this, SIGNAL(stopExecutor()), thread, SLOT(quit()));
    // Queue the thread for deletion (in this thread) when it is finished
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    // Default implementation of QThread::run() simply spins up an event loop in the thread,
    // which is what we want.
    thread->start();
}

void ConsolePage::scrollToEnd()
{
    QScrollBar *scrollbar = ui->messagesWidget->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void ConsolePage::dumpmarkethistory()
{
    // get market history from empo ex api
    QUrl url("https://api.empoex.com/markethistory/XQN-BTC");
    QNetworkRequest request;
    request.setUrl(url);
    qDebug() << "dumpmarkethistory()";
    QNetworkReply* currentReply = networkManager.get(request);
    connect(currentReply, SIGNAL(finished()), this, SLOT(eeHistoryReplyFinished()));
}

void ConsolePage::marketdata()
{
    // get market history from empo ex api
    QUrl url("https://api.empoex.com/marketinfo/XQN-BTC");
    QNetworkRequest request;
    request.setUrl(url);
    qDebug() << "dumpmarkethistory()";
    QNetworkReply* currentReply = networkManager.get(request);
    connect(currentReply, SIGNAL(finished()), this, SLOT(eeMktDataReplyFinished()));
}

void ConsolePage::eeMktDataReplyFinished()
{ 
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QString data = (QString) reply->readAll();
    //qDebug() << data;
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    // Now parse this JSON according to your needs !
    //QScriptValue entries = result.property("XQN-BTC");
    QScriptValueIterator it(result);

//[{"pairname":"XQN-BTC","last":"0.00002400","base_volume_24hr":"0.60486204","low":"0.00000989","high":"0.00002750","bid":"0.00001990",
//"ask":"0.00002400","open_buy_volume":"87,138.42828095","open_sell_volume":"5,642.11820899",
//"open_buy_volume_base":"0.25689740","open_sell_volume_base":"0.16165537","change":"+9.14%"}]
    QString tbl = "<b>EmpoEx XQN-BTC</b><br><br><table cellpadding=\"5\" width=\"100%\"><thead><tr><th></th><th>Last</th><th>% Change</th><th>24hr Volume</th><th>Low</th><th>High</th><th>Bid</th><th>Ask</th></tr></thead>";
    tbl = tbl + "<tbody>";
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

	    tbl = tbl + "<tr><td>";
	    if(change.startsWith("-"))
	    {
		tbl = tbl + "<img src=\":/icons/sad\" width=\"48\" height=\"48\"></td>";
	    }
	    else
	    {
		tbl = tbl + "<img src=\":/icons/smile\" width=\"48\" height=\"48\"></td>";
	    }
            tbl = tbl + "<td>" + last + "</td><td>" + change + "</td><td>" + volume + "</td><td>" + low + "</td><td>" + high + "</td><td>" + bid + "</td><td>" + ask + "</td></tr>";
	}
    }
    tbl = tbl + "</tbody></table>";
    qDebug() << "Returning market data table";
    message(CMD_REPLY, tbl, true); 
}

void ConsolePage::eeHistoryReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    qDebug() << "eeHistoryReplyFinished()";
    QString data = (QString) reply->readAll();
    QScriptEngine engine;
    QScriptValue result = engine.evaluate("value = " + data);
    QScriptValueIterator ite(result);
    while(ite.hasNext()) {
         ite.next();
        //qDebug() << ite.name() << ": " << ite.value().toString();
    }

    // {"XQN-BTC":[{"type":"Buy","date":1415845314,"amount":"142.80000000","price":"0.00002400","total":"0.00342720"},{"type":"Buy","date":1415845120,"amount":"202.98107194","price":"0.00002199","total":"0.00446355"},

    QString tbl = "<b>EmpoEx XQN-BTC</b><br><br><table cellpadding=\"5\" width=\"100%\"><thead><tr><th>Type</th><th>Date</th><th>Amount</th><th>Price</th><th>Total</th></tr></thead>";

    // Now parse this JSON according to your needs !
    QScriptValue entries = result.property("XQN-BTC");
    QScriptValueIterator it(entries);
    
    tbl = tbl + "<tbody>";
    while (it.hasNext()) {
        it.next();
        QScriptValue entry = it.value();
        QString type = entry.property("type").toString();
	QString date = entry.property("date").toString();
        QString amount = QString::number(entry.property("amount").toNumber(), 'f', 8);
        QString price = QString::number(entry.property("price").toNumber(), 'f', 8);
        QString total = QString::number(entry.property("total").toNumber(), 'f', 8);

        tbl = tbl + "<tr><td>" + type + "</td><td>" + date + "</td><td>" + amount + "</td><td>" + price + "</td><td>" + total + "</td></tr>";
    }
    tbl = tbl + "</tbody></table>";
    qDebug() << "Returning market data table";
    message(CMD_REPLY, tbl, true); 
}
