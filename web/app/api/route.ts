const log = (message: string) => {
    const timestamp = new Date().toISOString();
    console.log(`${timestamp} - ${message}`);
};

const CACHE_TTL_MS = BigInt(30 * 60 * 1000);

export const dynamic = "force-dynamic";

let cache: any = null;
let cacheTimestamp: bigint = BigInt(0);

export async function GET() {
    const apiKey = process.env.OPEN_WEATHER_API_KEY;

    if (!apiKey) {
        return new Response('Missing API key', { status: 500 });
    }

    const city = process.env.OPEN_WEATHER_CITY;

    if (!city) {
        return new Response('Missing city', { status: 500 });
    }

    const now = process.hrtime.bigint() / BigInt(1e6);
    if (cache && now - cacheTimestamp < CACHE_TTL_MS) {
        log(`Using cache from ${(now-cacheTimestamp)/BigInt(1000)} seconds ago..`);
    } else {
        log(`Fetching new data, cache empty or stale...`);

        const [weatherRes, forecastRes] = await Promise.all([
            fetch(`https://api.openweathermap.org/data/2.5/weather?q=${city}&APPID=${apiKey}&mode=json&units=metric&lang=EN`),
            fetch(`https://api.openweathermap.org/data/2.5/forecast?q=${city}&APPID=${apiKey}&mode=json&units=metric&lang=EN`)
        ]);

        const current = await weatherRes.json();
        const forecast = await forecastRes.json();

        cache = { current, forecast };
        cacheTimestamp = now;
    }

    return Response.json(cache);
}
