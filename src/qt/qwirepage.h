#ifndef QWIREPAGE_H
#define QWIREPAGE_H

#include <QWidget>
#include <QNetworkProxyFactory>
#include <QWebSettings>
#include <QWebView>

namespace Ui {
    class QWirePage;
}

class QWirePage : public QWidget
{
    Q_OBJECT

public:
    explicit QWirePage(QWidget *parent = 0);
    ~QWirePage();
    void loadVideoFeed();

private:
    Ui::QWirePage *ui;
    QWebView *webView;

};

#endif // QWIREPAGE_H
