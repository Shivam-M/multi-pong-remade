plugins {
    id("java")
    id("com.google.protobuf") version "0.9.4"
}

protobuf {
    protoc {
        artifact = "com.google.protobuf:protoc:3.25.3"
    }
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
    implementation("com.google.protobuf:protobuf-java:3.25.3")
}

tasks.test {
    useJUnitPlatform()
}