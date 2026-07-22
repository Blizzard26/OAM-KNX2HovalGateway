pipeline {
    agent { label 'platformio' }

    options {
        timeout(time: 30, unit: 'MINUTES')
        disableConcurrentBuilds()
    }

    stages {
        stage('Submodules') {
            steps {
                // The multibranch git branch source (seed.groovy, APP-jenkins repo)
                // has no submodule trait configured, and this repo pulls its OpenKNX
                // libs under lib/ as git submodules rather than via a restore script
                // - see CLAUDE.md's "Dependency layout" section.
                sh 'git submodule update --init --recursive'
            }
        }

        stage('Native Unit Tests') {
            steps {
                // Suites must run one at a time - see CLAUDE.md's "Known PlatformIO
                // Core bug" section. Add a line here for each new suite under test/.
                sh 'pio test -c test_platformio.ini -f test_hovalcrc'
            }
        }

        stage('Build Firmware') {
            steps {
                // Compiles directly via PlatformIO rather than
                // scripts/Build-Release.ps1 - that wrapper additionally needs pwsh,
                // OpenKNXproducer/ETS and Windows-only zip tooling to produce the
                // knxprod/release package, none of which the 'platformio' agent has.
                // include/versions.h is still generated correctly, since that's
                // PlatformIO's own extra_scripts hook
                // (lib/OGM-Common/scripts/pio/prepare.py), not part of the PS wrapper.
                sh 'pio run -e dev_KNX2HOVALGATEWAY'
                sh 'pio run -e release_KNX2HOVALGATEWAY'
            }
        }

        stage('Archive') {
            steps {
                archiveArtifacts artifacts: '.pio/build/*/firmware.uf2', fingerprint: true
            }
        }
    }
}
