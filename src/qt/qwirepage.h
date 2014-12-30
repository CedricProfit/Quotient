#ifndef QWIREPAGE_H
#define QWIREPAGE_H

#include <QWidget>
#include <QNetworkProxyFactory>
#include <QWebSettings>
#include <QWebView>
#include <QNetworkAccessManager>
#include <QVBoxLayout>

namespace Ui {
    class QWirePage;
}

class QWirePage : public QWidget
{
    Q_OBJECT

public:
    explicit QWirePage(QWidget *parent = 0);
    ~QWirePage();
    void loadFeed();
    void loadNewsFeed();
    void loadIndexFeed();
    void loadVideoFeed();

public slots:
    void updateFeed(const QString &hash, int status);

private slots:
    void getImgReplyFinished();
    void on_refreshButton_clicked();

private:
    Ui::QWirePage *ui;
    QNetworkAccessManager *manager;
    QVBoxLayout *videos;
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
    void remove(QLayout* layout);
};

#endif // QWIREPAGE_H
