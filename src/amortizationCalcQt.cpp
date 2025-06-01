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
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QToolTip>
#include <QTimer>
#include <QMouseEvent>
#include <limits>

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

    exportButton = new QPushButton("Export CSV");
    leftLayout->addWidget(exportButton);

    connect(exportButton, &QPushButton::clicked, this, &AmortizationCalc::exportCsv);

    tooltipTimer = new QTimer(this);
    tooltipTimer->setInterval(200); // refresh every 200ms
    connect(tooltipTimer, &QTimer::timeout, this, [this]() {
        if (tooltipActive && !lastTooltipText.isEmpty()) {
            QToolTip::showText(lastTooltipPos, lastTooltipText, chartView);
        }
    });

    auto makeHoverHandler = [this](const QString &label) {
        return [this, label](const QPointF &point, bool state) {
            if (state) {
                lastTooltipText = QString("%1\nX: %2\nY: %3")
                    .arg(label)
                    .arg(point.x())
                    .arg(point.y(), 0, 'f', 2);
                lastTooltipPos = QCursor::pos();
                QToolTip::showText(lastTooltipPos, lastTooltipText, chartView);
                tooltipActive = true;
                if (!tooltipTimer->isActive())
                    tooltipTimer->start();
            } else {
                lastTooltipText.clear();
                tooltipActive = false;
                tooltipTimer->stop();
                QToolTip::hideText();
            }
        };
    };

    connect(principalSeries, &QLineSeries::hovered, this, makeHoverHandler("Principal"));
    connect(interestSeries, &QLineSeries::hovered, this, makeHoverHandler("Interest"));
    connect(totalSeries, &QLineSeries::hovered, this, makeHoverHandler("Total"));

    chartView->setMouseTracking(true);
    chartView->viewport()->setMouseTracking(true);
    chartView->viewport()->installEventFilter(this);
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
    bool useYears = (termTypeBox->currentText() == "Years");
    if (useYears) {
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
    if (useYears) {
        int totalYears = (months + 11) / 12;
        for (int year = 1; year <= totalYears; ++year) {
            int lastMonthOfYear = std::min(year * 12, months);
            // Accumulate up to the end of this year (cumulative)
            for (int row = (year - 1) * 12; row < lastMonthOfYear; ++row) {
                double p = table->item(row, 2) ? table->item(row, 2)->text().toDouble() : 0.0;
                double in = table->item(row, 3) ? table->item(row, 3)->text().toDouble() : 0.0;
                runningPrincipal += p;
                runningInterest += in;
            }
            principalSeries->append(year, runningPrincipal);
            interestSeries->append(year, runningInterest);
            totalSeries->append(year, runningPrincipal + runningInterest);
        }
    } else {
        runningPrincipal = 0.0;
        runningInterest = 0.0;
        for (int row = 0; row < months; ++row) {
            double p = table->item(row, 2) ? table->item(row, 2)->text().toDouble() : 0.0;
            double in = table->item(row, 3) ? table->item(row, 3)->text().toDouble() : 0.0;
            runningPrincipal += p;
            runningInterest += in;
            principalSeries->append(row + 1, runningPrincipal);
            interestSeries->append(row + 1, runningInterest);
            totalSeries->append(row + 1, runningPrincipal + runningInterest);
        }
    }

    // Update axis labels and ticks
    QValueAxis *axisX = qobject_cast<QValueAxis *>(chartView->chart()->axisX());
    if (axisX) {
        if (useYears) {
            axisX->setTitleText("Year");
            axisX->setLabelFormat("%d");
            int totalYears = (months + 11) / 12;
            axisX->setRange(1, totalYears);
            axisX->setTickInterval(10); // 10 years per tick
        } else {
            axisX->setTitleText("Month");
            axisX->setLabelFormat("%d");
            axisX->setRange(1, months);
            axisX->setTickInterval(10); // 10 months per tick
        }
    }
    QValueAxis *axisY = qobject_cast<QValueAxis *>(chartView->chart()->axisY());
    if (axisY) {
        axisY->setRange(0, runningPrincipal + runningInterest);
    }
} // <-- This closes AmortizationCalc::calculate()

void AmortizationCalc::exportCsv() {
    QString fileName = QFileDialog::getSaveFileName(this, "Export Table as CSV", "", "CSV Files (*.csv)");
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        resultLabel->setText("Failed to open file for writing.");
        return;
    }

    QTextStream out(&file);

    // Write headers
    QStringList headers;
    for (int col = 0; col < table->columnCount(); ++col)
        headers << table->horizontalHeaderItem(col)->text();
    out << headers.join(",") << "\n";

    // Write data
    for (int row = 0; row < table->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem *item = table->item(row, col);
            rowData << (item ? item->text() : "");
        }
        out << rowData.join(",") << "\n";
    }
    file.close();
    resultLabel->setText("Exported table to: " + fileName);
}

bool AmortizationCalc::eventFilter(QObject *obj, QEvent *event) {
    if (obj == chartView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

            auto findNearest = [&](QLineSeries *series) -> std::pair<QPointF, double> {
                QPointF nearest;
                double minDist = std::numeric_limits<double>::max();
                for (const QPointF &pt : series->points()) {
                    QPointF ptPixel = chartView->chart()->mapToPosition(pt, series);
                    double dist = std::hypot(ptPixel.x() - mouseEvent->pos().x(), ptPixel.y() - mouseEvent->pos().y());
                    if (dist < minDist) {
                        minDist = dist;
                        nearest = pt;
                    }
                }
                return {nearest, minDist};
            };

            const double threshold = 10.0; // pixels

            auto [pPt, pDist] = findNearest(principalSeries);
            auto [iPt, iDist] = findNearest(interestSeries);
            auto [tPt, tDist] = findNearest(totalSeries);

            double minDist = std::min({pDist, iDist, tDist});
            QString tip;
            if (minDist < threshold) {
                if (minDist == pDist) {
                    tip = QString("Principal\nX: %1\nY: %2").arg(pPt.x()).arg(pPt.y(), 0, 'f', 2);
                } else if (minDist == iDist) {
                    tip = QString("Interest\nX: %1\nY: %2").arg(iPt.x()).arg(iPt.y(), 0, 'f', 2);
                } else {
                    tip = QString("Total\nX: %1\nY: %2").arg(tPt.x()).arg(tPt.y(), 0, 'f', 2);
                }
                lastTooltipText = tip;
                lastTooltipPos = mouseEvent->globalPos();
                tooltipActive = true;
                QToolTip::showText(lastTooltipPos, lastTooltipText, chartView);
                if (!tooltipTimer->isActive())
                    tooltipTimer->start();
            } else {
                tooltipActive = false;
                tooltipTimer->stop();
                QToolTip::hideText();
            }
            return true;
        }
        if (event->type() == QEvent::Leave) {
            tooltipActive = false;
            tooltipTimer->stop();
            QToolTip::hideText();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    AmortizationCalc window;
    window.setWindowTitle("Amortization Calculator");
    window.resize(700, 500);
    window.show();
    return app.exec();
}