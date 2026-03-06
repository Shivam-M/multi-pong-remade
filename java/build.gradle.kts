plugins {
    id("java")
    id("com.google.protobuf") version "0.9.4"
    id("org.openjfx.javafxplugin") version "0.1.0"
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:4.28.2"
    }
}

javafx {
    version = "21"
    modules = listOf("javafx.controls")
}

sourceSets {
    main {
        proto {
            srcDir("../_protobufs")
        }
    }
}

group = "com.sm"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(platform("org.junit:junit-bom:5.10.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    implementation("com.google.protobuf:protobuf-java:4.28.2")
    implementation("org.slf4j:slf4j-api:2.0.12")
    implementation("ch.qos.logback:logback-classic:1.4.14")
}

tasks.test {
    useJUnitPlatform()
}