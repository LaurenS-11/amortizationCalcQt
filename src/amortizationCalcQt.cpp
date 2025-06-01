#include "amortizationCalcQt.h"
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QFormLayout>
#include <QDoubleValidator>
#include <QVBoxLayout>
#include <QHeaderView>
#include <cmath>
#include <QStringList>
#include <QLocale>
#include <QApplication>

AmortizationCalc::AmortizationCalc(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QHBoxLayout(this);

    // Left side: form and table in a vertical layout
    auto *leftLayout = new QVBoxLayout();

    auto *form = new QFormLayout();
    principalEdit = new QLineEdit();
    principalEdit->setValidator(new QDoubleValidator(0, 1e9, 2, this));
    rateEdit = new QLineEdit();
    rateEdit->setValidator(new QDoubleValidator(0, 100, 4, this));
    termEdit = new QLineEdit();
    termEdit->setValidator(new QDoubleValidator(0, 1200, 2, this)); // allow up to 1200 months
    termTypeBox = new QComboBox();
    termTypeBox->addItem("Years");
    termTypeBox->addItem("Months");

    form->addRow("Principal ($):", principalEdit);
    form->addRow("Annual Interest Rate (%):", rateEdit);
    termLabel = new QLabel("Term:");
    auto *termRowLayout = new QHBoxLayout();
    termRowLayout->addWidget(termEdit);
    termRowLayout->addWidget(termTypeBox);
    form->addRow(termLabel, termRowLayout);

    leftLayout->addLayout(form);

    calcButton = new QPushButton("Calculate");
    leftLayout->addWidget(calcButton);

    resultLabel = new QLabel();
    leftLayout->addWidget(resultLabel);

    totalInterestLabel = new QLabel();
    leftLayout->addWidget(totalInterestLabel);

    monthsPaidLabel = new QLabel();
    leftLayout->addWidget(monthsPaidLabel);

    totalPaidLabel = new QLabel(); // <-- Add this line
    leftLayout->addWidget(totalPaidLabel); // <-- Add this line

    table = new QTableWidget();
    table->setColumnCount(6); // <-- Change to 6 columns
    QStringList headers = {"Payment #", "Payment", "Principal", "Interest", "Balance", "One-Time Payment"}; // <-- Add column
    table->setHorizontalHeaderLabels(headers);

    // Left-justify all column headers
    for (int i = 0; i < headers.size(); ++i) {
        QTableWidgetItem *headerItem = table->horizontalHeaderItem(i);
        if (headerItem) {
            headerItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    leftLayout->addWidget(table);

    mainLayout->addLayout(leftLayout, 1);   // half width

    // Right side: chart
    principalSeries = new QLineSeries();
    principalSeries->setName("Principal Paid");
    interestSeries = new QLineSeries();
    interestSeries->setName("Interest Paid");
    totalSeries = new QLineSeries();
    totalSeries->setName("Total Paid");

    QChart *chart = new QChart();
    chart->addSeries(principalSeries);
    chart->addSeries(interestSeries);
    chart->addSeries(totalSeries);
    chart->setTitle("Amortization Over Time");

    // Set up integer x-axis for months
    QValueAxis *axisX = new QValueAxis;
    axisX->setTitleText("Month");
    axisX->setLabelFormat("%d"); // integer labels
    axisX->setTickType(QValueAxis::TicksDynamic);
    axisX->setTickInterval(10); // Show ticks every 10 months
    axisX->setMinorTickCount(0);
    axisX->setRange(1, 12); // will be updated in calculate()

    // Make axis label and title font larger
    QFont labelFont = axisX->labelsFont();
    labelFont.setPointSize(12); // Adjust as needed
    axisX->setLabelsFont(labelFont);

    QFont titleFont = axisX->titleFont();
    titleFont.setPointSize(14); // Adjust as needed
    axisX->setTitleFont(titleFont);

    QValueAxis *axisY = new QValueAxis;
    axisY->setTitleText("Amount ($)");
    axisY->setLabelFormat("%.2f");

    chart->setAxisX(axisX, principalSeries);
    chart->setAxisX(axisX, interestSeries);
    chart->setAxisX(axisX, totalSeries);
    chart->setAxisY(axisY, principalSeries);
    chart->setAxisY(axisY, interestSeries);
    chart->setAxisY(axisY, totalSeries);

    chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    mainLayout->addWidget(chartView, 1);

    connect(calcButton, &QPushButton::clicked, this, &AmortizationCalc::calculate);

    connect(termTypeBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        termLabel->setText(QString("Term (%1):").arg(text));
    });
}

void AmortizationCalc::calculate() {
    // Remove commas from input before conversion
    QString principalStr = principalEdit->text().remove(',');
    QString rateStr = rateEdit->text().remove(',');
    QString termStr = termEdit->text().remove(',');

    double principal = principalStr.toDouble();
    double annualRate = rateStr.toDouble();

    double termValue = termStr.toDouble();
    int months = 0;
    if (termTypeBox->currentText() == "Years") {
        months = static_cast<int>(termValue * 12);
    } else {
        months = static_cast<int>(termValue);
    }

    if (principal <= 0 || annualRate <= 0 || months <= 0) {
        resultLabel->setText("Please enter valid values.");
        table->setRowCount(0);
        totalInterestLabel->clear();
        return;
    }

    double monthlyRate = annualRate / 12.0 / 100.0;
    double monthlyPayment = (principal * monthlyRate) / (1 - std::pow(1 + monthlyRate, -months));

    table->setRowCount(months);

    double balance = principal;
    double totalInterest = 0.0;

    int i = 0;
    for (; i < months; ++i) {
        double interest = balance * monthlyRate;
        double principalPaid = monthlyPayment - interest;

        // Read one-time payment from table if present
        double oneTimePayment = 0.0;
        QTableWidgetItem *otpItem = table->item(i, 5);
        if (otpItem) {
            // Remove commas before conversion
            oneTimePayment = otpItem->text().remove(',').toDouble();
        }

        if (i == months - 1) { // last payment adjustment
            principalPaid = balance;
            monthlyPayment = principalPaid + interest;
        }

        balance -= principalPaid;
        balance -= oneTimePayment; // <-- Subtract one-time payment

        if (balance < 0) balance = 0;

        totalInterest += interest;

        table->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        table->setItem(i, 1, new QTableWidgetItem(QString::number(monthlyPayment, 'f', 2)));
        table->setItem(i, 2, new QTableWidgetItem(QString::number(principalPaid, 'f', 2)));
        table->setItem(i, 3, new QTableWidgetItem(QString::number(interest, 'f', 2)));
        table->setItem(i, 4, new QTableWidgetItem(QString::number(std::max(0.0, balance), 'f', 2)));

        // Make the one-time payment cell editable
        QTableWidgetItem *otpEdit = new QTableWidgetItem();
        otpEdit->setFlags(otpEdit->flags() | Qt::ItemIsEditable);
        if (otpItem) otpEdit->setText(otpItem->text());
        table->setItem(i, 5, otpEdit);

        // Update chart series
        principalSeries->append(i + 1, principalPaid);
        interestSeries->append(i + 1, interest);
        totalSeries->append(i + 1, principalPaid + interest);

        if (balance <= 0) {
            // Fill remaining rows with zeros if loan is paid off early
            for (int j = i + 1; j < months; ++j) {
                table->setItem(j, 0, new QTableWidgetItem(QString::number(j + 1)));
                for (int k = 1; k < 5; ++k) { // Only columns 1-4 get zeros
                    table->setItem(j, k, new QTableWidgetItem("0.00"));
                }
                // For the "One-Time Payment" column (index 5), leave blank but editable
                QTableWidgetItem *otpEdit = new QTableWidgetItem();
                otpEdit->setFlags(otpEdit->flags() | Qt::ItemIsEditable);
                table->setItem(j, 5, otpEdit);
            }
            break;
        }
    }

    QLocale locale = QLocale::system();

    totalInterestLabel->setText(
        QString("Total Interest Paid: $%1")
            .arg(locale.toString(totalInterest, 'f', 2))
    );
    double paidYears = (i + 1) / 12.0;
    monthsPaidLabel->setText(
        QString("Total Months Until Paid Off: %1 (%2 years)")
            .arg(i + 1)
            .arg(QString::number(paidYears, 'f', 2))
    );
    totalPaidLabel->setText(
        QString("Total Principal + Interest Paid: $%1")
            .arg(locale.toString(principal + totalInterest, 'f', 2))
    );
    resultLabel->setText(
        QString("Monthly Payment: $%1")
            .arg(locale.toString(monthlyPayment, 'f', 2))
    );

    principalSeries->clear();
    interestSeries->clear();
    totalSeries->clear();

    double runningPrincipal = 0.0;
    double runningInterest = 0.0;
    for (int row = 0; row < months; ++row) {
        double p = table->item(row, 2) ? table->item(row, 2)->text().toDouble() : 0.0;
        double in = table->item(row, 3) ? table->item(row, 3)->text().toDouble() : 0.0;
        runningPrincipal += p;
        runningInterest += in;
        principalSeries->append(row + 1, runningPrincipal);
        interestSeries->append(row + 1, runningInterest);
        totalSeries->append(row + 1, runningPrincipal + runningInterest);
    }
    chartView->chart()->axes(Qt::Horizontal).first()->setRange(1, months);
    chartView->chart()->axes(Qt::Vertical).first()->setRange(0, runningPrincipal + runningInterest);

    // After updating the chart data, update the axis range and tick interval:
    QValueAxis *axisX = qobject_cast<QValueAxis *>(chartView->chart()->axisX());
    if (axisX) {
        axisX->setRange(1, months);
        axisX->setTickInterval(10); // Always show ticks every 10 months
    }
    QValueAxis *axisY = qobject_cast<QValueAxis *>(chartView->chart()->axisY());
    if (axisY) {
        axisY->setRange(0, runningPrincipal + runningInterest);
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    AmortizationCalc window;
    window.setWindowTitle("Amortization Calculator");
    window.resize(700, 500);
    window.show();
    return app.exec();
}