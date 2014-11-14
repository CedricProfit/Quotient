#include "profitexplorerpage.h"
#include "ui_profitexplorerpage.h"

#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>

ProfitExplorerPage::ProfitExplorerPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProfitExplorerPage)
{
    ui->setupUi(this);

}

ProfitExplorerPage::~ProfitExplorerPage()
{
    delete ui;
}
