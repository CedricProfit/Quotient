#include "qwirepage.h"
#include "ui_qwirepage.h"

#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QNetworkProxyFactory>
#include <QWebSettings>
#include <QWebView>

QWirePage::QWirePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QWirePage)
{
    ui->setupUi(this);

    loadVideoFeed();
}

void QWirePage::loadVideoFeed()
{
    QNetworkProxyFactory::setUseSystemConfiguration (true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::AutoLoadImages, true);
    webView = new QWebView();
    webView->setFixedWidth(290);
    webView->setObjectName("webView");
    webView->setStyleSheet("#webView { width:290px; background-color: #202020; }");
    //webView->load(QUrl("https://www.youtube.com/watch?v=w90HbXT_Siw"));
    QString html = "<html><body style=\"margin:0;padding:0;\"><table cellpadding=\"5\"><tr><td style=\"background-color:#4CFF00;\"><h3 style=\"font-family:Courier New;color:#202020;\">Daily Highlight</h3><iframe width=\"260\" height=\"157\" src=\"http://www.youtube.com/embed/w90HbXT_Siw\" frameborder=\"0\" allowfullscreen></iframe></td></tr><tr><td><iframe width=\"260\" height=\"157\" src=\"http://www.youtube.com/embed/w90HbXT_Siw\" frameborder=\"0\" allowfullscreen></iframe></td></tr><tr><td><iframe width=\"260\" height=\"157\" src=\"http://www.youtube.com/embed/w90HbXT_Siw\" frameborder=\"0\" allowfullscreen></iframe></td></tr><tr><td><iframe width=\"260\" height=\"157\" src=\"http://www.youtube.com/embed/w90HbXT_Siw\" frameborder=\"0\" allowfullscreen></iframe></td></tr><tr><td><iframe width=\"260\" height=\"157\" src=\"http://www.youtube.com/embed/w90HbXT_Siw\" frameborder=\"0\" allowfullscreen></iframe></td></tr><tr><td><iframe width=\"260\" height=\"157\" src=\"http://www.youtube.com/embed/w90HbXT_Siw\" frameborder=\"0\" allowfullscreen></iframe></td></tr></table></body></html>";
    webView->setHtml(html);
    ui->verticalLayout_2->addWidget(webView);
}

QWirePage::~QWirePage()
{
    delete webView;
    delete ui;
}
