// #ifndef DEVICELISTENER_H
// #define DEVICELISTENER_H
// #include <QBluetoothDeviceInfo>
// #include <QLowEnergyAdvertisingData>
// #include <QLowEnergyController>
// #include <QLowEnergyServiceData>
// #include <QObject>

// class DeviceListener : public QObject
// {
//     Q_OBJECT

// public:
//     DeviceListener();
//     ~DeviceListener();

// private slots:
//     void clientConnected();
//     void clientDisconnected();

// private:
//     void setupVirtualKeyboard();

//     QLowEnergyAdvertisingData m_advertisingData;
//     QLowEnergyServiceData m_hidServiceData;

//     QLowEnergyController *m_leController = nullptr;
//     QLowEnergyService *m_service = nullptr;
//     QLowEnergyCharacteristic
//         m_inputReportChar; // Keep a reference to the characteristic
// };

// #endif // DEVICELISTENER_H
