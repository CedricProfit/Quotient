#include "qwirepage.h"
#include "ui_qwirepage.h"

#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>

QWirePage::QWirePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QWirePage)
{
    ui->setupUi(this);

}

QWirePage::~QWirePage()
{
    delete ui;
}
