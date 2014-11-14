#ifndef PROFITEXPLORERPAGE_H
#define PROFITEXPLORERPAGE_H

#include <QWidget>

namespace Ui {
    class ProfitExplorerPage;
}

class ProfitExplorerPage : public QWidget
{
    Q_OBJECT

public:
    explicit ProfitExplorerPage(QWidget *parent = 0);
    ~ProfitExplorerPage();

private:
    Ui::ProfitExplorerPage *ui;

};

#endif // PROFITEXPLORERPAGE_H
