#ifndef PROFITEXPLORERPAGE_H
#define PROFITEXPLORERPAGE_H

#include <QWidget>
#include <QTimer>
#include "qcustomplot.h"

namespace Ui {
    class ProfitExplorerPage;
}

class ProfitExplorerPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProfitExplorerPage(QWidget *parent = 0);
    ~ProfitExplorerPage();
    void loadStakeChart(bool firstRun);

private:
    Ui::ProfitExplorerPage *ui;
    QTimer updateTimer;

private slots:
    virtual void updateTimer_timeout();
    void on_recomputeButton_clicked();
};

#endif // PROFITEXPLORERPAGE_H
