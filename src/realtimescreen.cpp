#include "realtimescreen.h"
#include "appcontext.h"
#include "iDescriptor.h"
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/screenshotr.h>

#include <exception>
#include <stdio.h>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifndef _WIN32
#include <signal.h>
#endif
static void get_image_filename(char *imgdata, char **filename)
{
    // If the provided filename already has an extension, use it as is.
    if (*filename) {
        char *last_dot = strrchr(*filename, '.');
        if (last_dot && !strchr(last_dot, '/')) {
            return;
        }
    }

    // Find the appropriate file extension for the filename.
    const char *fileext = NULL;
    if (memcmp(imgdata, "\x89PNG", 4) == 0) {
        fileext = ".png";
    } else if (memcmp(imgdata, "MM\x00*", 4) == 0) {
        fileext = ".tiff";
    } else {
        printf("WARNING: screenshot data has unexpected image format.\n");
        fileext = ".dat";
    }

    // If a filename without an extension is provided, append the extension.
    // Otherwise, generate a filename based on the current time.
    char *basename = NULL;
    if (*filename) {
        basename = (char *)malloc(strlen(*filename) + 1);
        strcpy(basename, *filename);
        free(*filename);
        *filename = NULL;
    } else {
        time_t now = time(NULL);
        basename = (char *)malloc(32);
        strftime(basename, 31, "screenshot-%Y-%m-%d-%H-%M-%S", gmtime(&now));
    }

    // Ensure the filename is unique on disk.
    char *unique_filename =
        (char *)malloc(strlen(basename) + strlen(fileext) + 7);
    sprintf(unique_filename, "%s%s", basename, fileext);
    int i;
    for (i = 2; i < (1 << 16); i++) {
        if (access(unique_filename, F_OK) == -1) {
            *filename = unique_filename;
            break;
        }
        sprintf(unique_filename, "%s-%d%s", basename, i, fileext);
    }
    if (!*filename) {
        free(unique_filename);
    }
    free(basename);
}

RealtimeScreen::RealtimeScreen(QString udid, QWidget *parent)
    : QWidget{parent}, timer(nullptr), capturing(false), shotrClient(nullptr)
{
    device = AppContext::sharedInstance()->getDevice(udid.toStdString());
    if (!device) {
        qDebug() << "Device not found for udid:" << udid;
        return;
    }

    qDebug() << "Creating RealtimeScreen for device:"
             << device->deviceInfo.productType.c_str();
    connect(AppContext::sharedInstance(), &AppContext::deviceRemoved, this,
            [this, udid](const std::string &removed_uuid) {
                if (udid.toStdString() == removed_uuid) {
                    this->hide();
                    this->deleteLater();
                }
            });
    // connect(AppContext::sharedInstance(), &AppContext::onDeviceRemoved, this,
    // [this, udid](const QString &removed_uuid) {
    // });

    QVBoxLayout *layout = new QVBoxLayout(this);
    QWidget *mainWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout();

    QLabel *titleLabel = new QLabel("Real-time Screen " + udid);
    mainLayout->addWidget(titleLabel);

    // Frame rate control
    QSpinBox *fpsSpinBox = new QSpinBox();
    fpsSpinBox->setRange(1, 60);
    fpsSpinBox->setValue(5);
    fpsSpinBox->setSuffix(" FPS");
    mainLayout->addWidget(fpsSpinBox);

    // Start/Stop button
    QPushButton *startStopButton = new QPushButton("Start");
    mainLayout->addWidget(startStopButton);

    // Screenshot display
    imageLabel = new QLabel();
    imageLabel->setFixedSize(300, 600); // Adjust as needed
    imageLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(imageLabel);

    mainWidget->setLayout(mainLayout);
    layout->addWidget(mainWidget);
    setLayout(layout);

    // Timer for periodic screenshots
    QTimer *timer = new QTimer(this);
    timer->setInterval(1000 / fpsSpinBox->value());

    connect(fpsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this, timer](int value) { timer->setInterval(1000 / value); });

    connect(startStopButton, &QPushButton::clicked, this,
            [this, startStopButton, timer]() {
                capturing = !capturing;
                if (capturing) {
                    startStopButton->setText("Stop");
                    timer->start();
                } else {
                    startStopButton->setText("Start");
                    timer->stop();
                }
            });

    // lockdownd_client_free(device->lockdownClient);
    try {
        idevice_t device = NULL;
        lockdownd_client_t lckd = NULL;
        lockdownd_error_t ldret = LOCKDOWN_E_UNKNOWN_ERROR;
        // screenshotr_client_t shotr = NULL;
        lockdownd_service_descriptor_t service = NULL;
        int result = -1;
        const char *udid = NULL;
        int use_network = 0;
        char *filename = NULL;
        int c = 0;

        if (IDEVICE_E_SUCCESS !=
            idevice_new_with_options(&device, udid,
                                     (use_network) ? IDEVICE_LOOKUP_NETWORK
                                                   : IDEVICE_LOOKUP_USBMUX)) {
            if (udid) {
                printf("No device found with udid %s.\n", udid);
            } else {
                printf("No device found.\n");
            }
        }

        if (LOCKDOWN_E_SUCCESS != (ldret = lockdownd_client_new_with_handshake(
                                       device, &lckd, "testing"))) {
            idevice_free(device);
            printf("ERROR: Could not connect to lockdownd, error code %d\n",
                   ldret);
        }

        lockdownd_error_t lerr =
            lockdownd_start_service(lckd, SCREENSHOTR_SERVICE_NAME, &service);
        lockdownd_client_free(lckd);
        if (lerr == LOCKDOWN_E_SUCCESS) {
            if (screenshotr_client_new(device, service, &shotrClient) !=
                SCREENSHOTR_E_SUCCESS) {
                printf("Could not connect to screenshotr!\n");
            } else {
                connect(timer, &QTimer::timeout, this,
                        &RealtimeScreen::updateScreenshot);
                // char *imgdata = NULL;
                // uint64_t imgsize = 0;
                // if (screenshotr_take_screenshot(shotrClient, &imgdata,
                // &imgsize) == SCREENSHOTR_E_SUCCESS)
                // {
                //     get_image_filename(imgdata, &filename);
                //     if (!filename)
                //     {
                //         printf("FATAL: Could not find a unique filename!\n");
                //     }
                //     else
                //     {
                //         FILE *f = fopen(filename, "wb");
                //         if (f)
                //         {
                //             if (fwrite(imgdata, 1, (size_t)imgsize, f) ==
                //             (size_t)imgsize)
                //             {
                //                 printf("Screenshot saved to %s\n", filename);
                //                 result = 0;
                //             }
                //             else
                //             {
                //                 printf("Could not save screenshot to file
                //                 %s!\n", filename);
                //             }
                //             fclose(f);
                //         }
                //         else
                //         {
                //             printf("Could not open %s for writing: %s\n",
                //             filename, strerror(errno));
                //         }
                //     }
                // }
                // else
                // {
                //     printf("Could not get screenshot!\n");
                // }
                // screenshotr_client_free(shotrClient);
            }
        } else {
            printf("Could not start screenshotr service: %s\nRemember that you "
                   "have to mount the Developer disk image on your device if "
                   "you want to use the screenshotr service.\n",
                   lockdownd_strerror(lerr));
        }

        // if (service)
        //     lockdownd_service_descriptor_free(service);

        // idevice_free(device);
        // free(filename);

    } catch (const std::exception &e) {
        fprintf(stderr, "Exception: %s\n", e.what());
    }
}

RealtimeScreen::~RealtimeScreen()
{
    if (timer)
        timer->stop();

    // Free resources here
    // if (shotrClient) screenshotr_client_free(*shotrClient);
}

void RealtimeScreen::updateScreenshot()
{
    try {
        qDebug() << "Updating screenshot...";
        qDebug() << "shotrClient:" << shotrClient;
        /* code */
        TakeScreenshotResult result = take_screenshot(shotrClient);
        qDebug() << "Result" << (!result.img.isNull() ? "success" : "failure");
        if (result.success && !result.img.isNull()) {
            imageLabel->setPixmap(
                QPixmap::fromImage(result.img)
                    .scaled(imageLabel->size(), Qt::KeepAspectRatio));
        }
    } catch (const std::exception &e) {
        qDebug() << "Exception in updateScreenshot:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception in updateScreenshot";
    }
}
