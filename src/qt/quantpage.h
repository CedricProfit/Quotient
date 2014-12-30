#ifndef QUANTPAGE_H
#define QUANTPAGE_H

#include <QWidget>
#include <QNetworkAccessManager>
#include "qcustomplot.h"

namespace Ui {
    class QuantPage;
}

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
QT_END_NAMESPACE


class QuantPage : public QWidget
{
    Q_OBJECT

public:
    explicit QuantPage(QWidget *parent = 0);
    ~QuantPage();

    void updateMarketData();
    void updateChart();
    void updateDepthChart();
    bool isClosing;

private slots:
    virtual void updateTimer_timeout();
    void eeHistoryReplyFinished();
    void eeMktDataReplyFinished();
    void trexHistoryReplyFinished();
    void trexMktDataReplyFinished();
    void trexMktHistoryReplyFinished();
    void eeMktHistoryReplyFinished();
    void on_refreshNowButton_clicked();

private:
    Ui::QuantPage *ui;
    QNetworkAccessManager *networkManager;
    QTimer updateTimer;
    void updateOrderBook();
};

#endif // QUANTPAGE_H
