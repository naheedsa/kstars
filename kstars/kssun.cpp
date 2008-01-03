/***************************************************************************
                          kssun.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kssun.h"

#include <math.h>
#include <qdatetime.h>

#include "ksutils.h"
#include "ksnumbers.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"

KSSun::KSSun( KStarsData *kd )
        : KSPlanet( kd, I18N_NOOP( "Sun" ), "sun.png", Qt::yellow, 1392000. /*diameter in km*/  )
{
    setMag( -26.73 );
}

bool KSSun::loadData() {
    OrbitDataColl odc;
    return (odm.loadData(odc, "earth") != 0);
}

bool KSSun::findGeocentricPosition( const KSNumbers *num, const KSPlanetBase *Earth ) {
    if (Earth) {
        //
        // For the precision we need, the earth's orbit is circular.
        // So don't bother to iterate like KSPlanet does. Just subtract
        // The current delay and recompute (once).
        //

        //The light-travel time delay, in millenia
        //0.0057755183 is the inverse speed of light, in days/AU
        double delay = (.0057755183 * Earth->rsun()) / 365250.0;
        //
        // MHH 2002-02-04 I don't like this. But it avoids code duplication.
        // Maybe we can find a better way.
        //
        const KSPlanet *pEarth = static_cast<const KSPlanet *>(Earth);
        EclipticPosition trialpos;
        pEarth->calcEcliptic(num->julianMillenia() - delay, trialpos);

        setEcLong( trialpos.longitude.Degrees() + 180.0 );
        setEcLong( ecLong()->reduce().Degrees() );
        setEcLat( -1.0*trialpos.latitude.Degrees() );

        setRearth( Earth->rsun() );

    } else {
        double sum[6];
        dms EarthLong, EarthLat; //heliocentric coords of Earth
        OrbitDataColl odc;
        double T = num->julianMillenia(); //Julian millenia since J2000
        double Tpow[6];

        Tpow[0] = 1.0;
        for (int i=1; i<6; ++i) {
            Tpow[i] = Tpow[i-1] * T;
        }
        //First, find heliocentric coordinates

        if ( ! odm.loadData(odc, "earth") ) return false;

        //Ecliptic Longitude
        for (int i=0; i<6; ++i) {
            sum[i] = 0.0;
            for (int j = 0; j < odc.Lon[i].size(); ++j) {
                sum[i] += odc.Lon[i][j].A * cos( odc.Lon[i][j].B + odc.Lon[i][j].C*T );
            }
            sum[i] *= Tpow[i];
            //kDebug() << name() << " : sum[" << i << "] = " << sum[i];
        }

        EarthLong.setRadians( sum[0] + sum[1] + sum[2] +
                              sum[3] + sum[4] + sum[5] );
        EarthLong.setD( EarthLong.reduce().Degrees() );

        //Compute Ecliptic Latitude
        for (int i=0; i<6; ++i) {
            sum[i] = 0.0;
            for (int j = 0; j < odc.Lat[i].size(); ++j) {
                sum[i] += odc.Lat[i][j].A * cos( odc.Lat[i][j].B + odc.Lat[i][j].C*T );
            }
            sum[i] *= Tpow[i];
        }


        EarthLat.setRadians( sum[0] + sum[1] + sum[2] + sum[3] +
                             sum[4] + sum[5] );

        //Compute Heliocentric Distance
        for (int i=0; i<6; ++i) {
            sum[i] = 0.0;
            for (int j = 0; j < odc.Dst[i].size(); ++j) {
                sum[i] += odc.Dst[i][j].A * cos( odc.Dst[i][j].B + odc.Dst[i][j].C*T );
            }
            sum[i] *= Tpow[i];
        }

        ep.radius = sum[0] + sum[1] + sum[2] + sum[3] + sum[4] + sum[5];
        setRearth( ep.radius );

        setEcLong( EarthLong.Degrees() + 180.0 );
        setEcLong( ecLong()->reduce().Degrees() );
        setEcLat( -1.0*EarthLat.Degrees() );
    }

    //Finally, convert Ecliptic coords to Ra, Dec.  Ecliptic latitude is zero, by definition
    EclipticToEquatorial( num->obliquity() );

    nutate(num);
    aberrate(num);

    // We obtain the apparent geocentric ecliptic coordinates. That is, after
    // nutation and aberration have been applied.
    EquatorialToEcliptic( num->obliquity() );

    //Determine the position angle
    findPA( num );

    //Set the angular size in arcmin
    setAngularSize( asin(physicalSize()/Rearth/AU_KM)*60.*180./dms::PI );

    return true;
}

