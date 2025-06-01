# Amortization Calculator Qt

A desktop amortization calculator built with Qt 6 and C++.  
It allows you to calculate and visualize loan amortization schedules, including support for one-time payments, and displays results in both tabular and chart form.

## Features

- Enter loan principal, annual interest rate, and term (in years or months)
- Select term units (years or months) with a pulldown next to the input
- Editable table for one-time payments per month (accepts commas in dollar amounts)
- Displays monthly payment, total interest paid, total paid, and months until paid off
- Interactive chart with x-axis in 10-month increments and readable labels
- All results update instantly when you click "Calculate"

## Build Instructions

### Prerequisites

- Qt 6 (with QtCharts module)
- CMake >= 3.16
- A C++17 compiler (e.g., GCC 9+)
- Linux (tested), should work on other platforms with Qt 6

### Build Steps

```sh
git clone <this-repo-url>
cd amortizationCalcQt
mkdir build
cd build
cmake ..
make
./amortizationCalcQt
```

## Usage

1. Enter the **Principal** (loan amount) and **Annual Interest Rate**.
2. Enter the **Term** and select **Years** or **Months** from the pulldown.
3. Click **Calculate**.
4. Optionally, enter one-time payments in the table (commas allowed, e.g., `1,000.00`).
5. View the updated table and chart.

## Notes

- The x-axis of the chart uses 10-month increments for readability.
- The "Term" label updates to reflect your unit selection.
- The table and chart update automatically after calculation.
