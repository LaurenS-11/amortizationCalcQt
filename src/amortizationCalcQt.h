#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QTimer>

class AmortizationCalc : public QWidget {
    Q_OBJECT

public:
    explicit AmortizationCalc(QWidget *parent = nullptr);

private slots:
    void calculate();
    void exportCsv();

private:
    QLineEdit *principalEdit;
    QLineEdit *rateEdit;
    QLineEdit *termEdit;
    QComboBox *typeBox;
    QComboBox *termTypeBox;
    QPushButton *calcButton;
    QPushButton *exportButton;
    QLabel *resultLabel;
    QLabel *totalInterestLabel;
    QLabel *monthsPaidLabel;
    QLabel *totalPaidLabel;
    QLabel *termLabel;
    QTableWidget *table;
    QChartView *chartView;
    QLineSeries *principalSeries;
    QLineSeries *interestSeries;
    QLineSeries *totalSeries;
    QTimer *tooltipTimer = nullptr;
    bool tooltipActive = false;
    QString lastTooltipText;
    QPoint lastTooltipPos;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};