# AWS IoT Core Setup Guide

To enable cloud telemetry for DesktopBuddy, you need to configure an AWS IoT "Thing" and retrieve the necessary certificates.

## 1. Create a Policy
1. Go to **AWS IoT Core** -> **Security** -> **Policies**.
2. Click **Create**.
3. Name it `DesktopBuddyPolicy`.
4. In the Policy Document, use the following broad policy for testing (restrict this in production!):
   * Action: `iot:*`
   * Resource ARN: `*`
   * Effect: `Allow`
5. Click **Create**.

## 2. Create the Thing & Certificates
1. Go to **Manage** -> **Things** and click **Create things**.
2. Select **Create single thing**, name it `ESP32-CAM-Tracker`.
3. Select **Auto-generate a new certificate**.
4. Attach the `DesktopBuddyPolicy` you created earlier.
5. **CRITICAL STEP**: Download all generated certificates immediately:
   * Device Certificate (`xxxx-certificate.pem.crt`)
   * Private Key (`xxxx-private.pem.key`)
   * Amazon Root CA 1 (`AmazonRootCA1.pem`)

## 3. Configure the Code
Open your `secrets.h` file (create it from `secrets.h.example`) and insert the contents of your downloaded certificates. 

*Note: You must include the `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----` headers exactly as they appear in the files.*

## 4. Find Your Endpoint
1. In the AWS IoT Core console, click **Settings** (bottom left).
2. Copy your **Endpoint** (it looks like `a2xxxxxxxxxxxx-ats.iot.region.amazonaws.com`).
3. Paste this into the `AWS_IOT_ENDPOINT` variable in `secrets.h`.

Once the ESP32 is flashed, you can view the live telemetry by going to **MQTT test client** in AWS IoT and subscribing to the topic: `esp32/face_tracker/data`.
