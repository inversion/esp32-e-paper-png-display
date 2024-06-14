"use client";

import Chart from "chart.js/auto";
import { format } from "date-fns";
import {
  useState,
  useEffect,
  useRef,
  RefObject,
} from "react";

async function getWeather() {
  const res = await fetch("api");
  const data = await res.json();

  return data;
}

function ms_to_mph(ms: number) {
  return ms * 2.23694;
}

function useForecastGraph(
  ref: RefObject<HTMLCanvasElement>,
  data: any,
  suggestedMax: number,
  dataHandler: (forecastItem: any) => Record<string, number>
) {
  const [chart, setChart] = useState<Chart | null>(null);

  useEffect(() => {
    if (!ref.current || !data) {
      return;
    }

    if (!chart) {
      const labels = data.forecast.list.map((item: any) => {
        const date = item.dt * 1000;
        return format(date, "EEE");
      });

      const dataPoints = data.forecast.list.map(dataHandler);

      setChart(
        new Chart(ref.current, {
          type: "line",
          data: {
            labels: labels,
            datasets: Object.keys(dataPoints[0]).map((key: string) => ({
              data: dataPoints.map((x: any) => x[key]),
              borderWidth: 1,
            })),
          },
          options: {
            // @ts-ignore
            pointStyle: false,
            maintainAspectRatio: false,
            plugins: {
              legend: {
                display: false,
              },
              tooltip: {
                enabled: false,
              },
            },
            scales: {
              x: {
                grid: { display: false },
              },
              y: {
                beginAtZero: true,
                grid: { display: false },
                suggestedMax,
              },
            },
          },
        })
      );
    }
  }, [ref, data, dataHandler]);
}

export default function Home() {
  const [data, setData] = useState<any>(null);

  useEffect(() => {
    async function fetchData() {
      const result = await getWeather();
      setData(result);
    }

    fetchData();
  }, []);

  const tempGraphRef = useRef<HTMLCanvasElement>(null);
  useForecastGraph(tempGraphRef, data,  30, (x: any) => ({
    feels_like: Math.round(x.main.feels_like),
    temp : Math.round(x.main.temp),
  }));
  const windGraphRef = useRef<HTMLCanvasElement>(null);
  useForecastGraph(windGraphRef, data,  25, (x: any) => ({
    speed: Math.round(ms_to_mph(x.wind.speed)),
    gust: Math.round(ms_to_mph(x.wind.gust)),
  }));
  const rainGraphRef = useRef<HTMLCanvasElement>(null);
  useForecastGraph(rainGraphRef, data, 10, (x: any) => ({
    rain: Math.round(x?.rain?.["3h"] ?? 0),
  }));

  if (!data) {
    return null;
  }

  const { current } = data;

  const date = current.dt * 1000;
  const formattedDate = format(date, "EEE do HH:mm");

  // const feelsTemp = Math.round(current.main.feels_like);
  const temperature = Math.round(current.main.temp);
  const minTemp = Math.round(current.main.temp_min);
  const maxTemp = Math.round(current.main.temp_max);
  const maxWind = Math.round(ms_to_mph(current.wind.speed));

  return (
    <div className="container">
        <div className="metric-summary">
          <h2>{formattedDate}</h2>
          <img
            src={`https://openweathermap.org/img/wn/${current.weather[0].icon}@2x.png`}
            alt="Weather icon"
          />
          <div className="temps">
            {temperature} ({minTemp}-{maxTemp}) &deg;C
          </div>
          <div className="windMax">{maxWind} mph</div>
        </div>
        <div className="metric-summary">
          <h1>Temp &deg;C</h1>
          <div className="graph">
            <canvas ref={tempGraphRef}></canvas>
          </div>
        </div>
        <div className="metric-summary">
          <h1>Wind (mph)</h1>
          <div className="graph">
            <canvas ref={windGraphRef}></canvas>
          </div>
        </div>
        <div className="metric-summary">
          <h1>Rain (mm)</h1>
          <div className="graph">
            <canvas ref={rainGraphRef}></canvas>
          </div>
        </div>
    </div>
  );
}
