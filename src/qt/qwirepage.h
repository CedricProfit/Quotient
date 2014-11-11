#ifndef QWIREPAGE_H
#define QWIREPAGE_H

#include <QWidget>

namespace Ui {
    class QWirePage;
}

class QWirePage : public QWidget
{
    Q_OBJECT

public:
    explicit QWirePage(QWidget *parent = 0);
    ~QWirePage();

private:
    Ui::QWirePage *ui;

};

#endif // QWIREPAGE_H
