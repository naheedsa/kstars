/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skypoint.h"
#include "ekos/ekos.h"
#include "indi/inditelescope.h"

#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QDialog>
#include <QBrush>
#include <QPen>

namespace Ui
{
class FramingAssistant;
}

namespace Ekos
{
class Scheduler;
class MosaicTilesManager;
class MosaicTilesScene;

class FramingAssistantUI : public QDialog
{
        Q_OBJECT
    public:

        FramingAssistantUI();
        ~FramingAssistantUI() override;

    public:
        enum
        {
            PAGE_EQUIPMENT,
            PAGE_SELECT_GRID,
            PAGE_ADJUST_GRID,
            PAGE_CREATE_JOBS
        };

    protected:
        /// @brief Camera information validity checker.
        bool isEquipmentValid() const;

        /// @brief Expected arcmin field width for the current number of tiles.
        double getTargetWFOV() const;

        /// @brief Expected arcmin field height for the current number of tiles.
        double getTargetHFOV() const;

        /// @brief Expected number of tiles for the current target field width.
        double getTargetMosaicW() const;

        /// @brief Expected number of tiles for the current target field height.
        double getTargetMosaicH() const;

        /**
         * @brief goAndSolve Go to current center, capture an image, and solve.
         */
        void goAndSolve();

        void createJobs();
        // Select sequence file
        void selectSequence();
        // Select jobs directory
        void selectDirectory();

    public slots:
        void updateTargetFOVFromGrid();
        void updateGridFromTargetFOV();
        void constructMosaic();
        void calculateFOV();
        void resetFOV();
        void fetchINDIInformation();
        void rewordStepEvery(int v);
        void setMountState(ISD::Telescope::Status value);
        void setAlignState(AlignState value);

    private:

        QString sanitize(const QString &name);
        SkyPoint m_CenterPoint;
        Ui::FramingAssistant *ui {nullptr};

        double renderedWFOV { 0 }, renderedHFOV { 0 };
        QTimer *m_DebounceTimer { nullptr };

        // Go and solve
        bool m_GOTOSolvePending {false};
        AlignState m_AlignState {ALIGN_IDLE};
        ISD::Telescope::Status m_MountState {ISD::Telescope::MOUNT_IDLE};

        // Equipment
        double m_FocalLength {0};
        QSize m_CameraSize;
        QSizeF m_PixelSize, m_cameraFOV, m_MosaicFOV;
        QSize m_GridSize {1, 1};
        double m_Overlap {10}, m_Rotation {0};
        QString m_JobsDirectory;
};
}
