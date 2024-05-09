import puppeteer from 'puppeteer';

import fs from 'fs';

const isRunningInDocker = fs.existsSync('/.dockerenv');

const log = (message: string) => {
    const timestamp = new Date().toISOString();
    console.log(`${timestamp} - ${message}`);
};


if (isRunningInDocker) {
    log('Running inside a Docker container');
} else {
    log('Not running inside a Docker container');
}


const takeScreenshot = async () => {
    const browser = await puppeteer.launch({
        defaultViewport: { width: 400, height: 300 },
        executablePath: isRunningInDocker ? '/usr/bin/google-chrome' : undefined,
//        args: isRunningInDocker ? ["--no-sandbox", "--disable-setuid-sandbox"] : undefined,
    });
    const page = await browser.newPage();

    const url = isRunningInDocker ? 'http://esp-web:3010' : 'http://localhost:3000';
    const screenshotPath = isRunningInDocker ? '/screenshot/screenshot.png' : 'screenshot.png';

    log(`Loading ${url}...`);
    await page.goto(url, { waitUntil: 'load' });

    log(`${url} loaded...`);
    await page.waitForSelector('.metric-summary');
    log(`selector found...`);
    // Wait for images to load
    await page.waitForNetworkIdle({ idleTime: 5000 });
    log(`network idle...`);
    await page.screenshot({ path: screenshotPath });
    log(`screenshot saved to ${screenshotPath}`);
    await browser.close();
    log('Browser closed');
};

// Take a screenshot immediately
takeScreenshot();

// Then take a screenshot every 5 minutes
setInterval(takeScreenshot, 5 * 60 * 1000);
