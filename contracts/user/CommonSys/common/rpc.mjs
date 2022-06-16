import { siblingUrl } from "/common/rootdomain.mjs";
import {
  privateStringToKeyPair,
  publicKeyPairToFracpack,
  signatureToFracpack,
} from "/common/keyConversions.mjs";
import hashJs from "https://cdn.skypack.dev/hash.js";

export { siblingUrl };

export class RPCError extends Error {
  constructor(message, trace) {
    super(message);
    this.trace = trace;
  }
}

export async function throwIfError(response) {
  if (!response.ok) throw new RPCError(await response.text());
  return response;
}

export async function get(url) {
  return await throwIfError(await fetch(url, { method: "GET" }));
}

export async function getText(url) {
  return await (await get(url)).text();
}

export async function getJson(url) {
  return await (await get(url)).json();
}

export async function postText(url, text) {
  return await throwIfError(
    await fetch(url, {
      method: "POST",
      headers: {
        "Content-Type": "text",
      },
      body: text,
    })
  );
}

export async function postGraphQL(url, graphql) {
  return await throwIfError(
    await fetch(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/graphql",
      },
      body: graphql,
    })
  );
}

export async function postJson(url, json) {
  return await throwIfError(
    await fetch(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(json),
    })
  );
}

export async function postArrayBuffer(url, arrayBuffer) {
  return await throwIfError(
    await fetch(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/octet-stream",
      },
      body: arrayBuffer,
    })
  );
}

export async function postTextGetJson(url, text) {
  return await (await postText(url, text)).json();
}

export async function postGraphQLGetJson(url, graphQL) {
  return await (await postGraphQL(url, graphQL)).json();
}

export async function postJsonGetJson(url, json) {
  return await (await postJson(url, json)).json();
}

export async function postJsonGetText(url, json) {
  return await (await postJson(url, json)).text();
}

export async function postJsonGetArrayBuffer(url, json) {
  return await (await postJson(url, json)).arrayBuffer();
}

export async function postArrayBufferGetJson(url, arrayBuffer) {
  return await (await postArrayBuffer(url, arrayBuffer)).json();
}

export async function packAction(baseUrl, action) {
  let { sender, contract, method, data, rawData } = action;
  if (!rawData) {
    rawData = uint8ArrayToHex(
      new Uint8Array(
        await postJsonGetArrayBuffer(
          siblingUrl(baseUrl, contract, "/pack_action/" + method),
          data
        )
      )
    );
  }
  return { sender, contract, method, rawData };
}

export async function packActions(baseUrl, actions) {
  return await Promise.all(
    actions.map((action) => packAction(baseUrl, action))
  );
}

export async function packTransaction(baseUrl, transaction) {
  return await postJsonGetArrayBuffer(
    baseUrl.replace(/\/+$/, "") + "/common/pack/Transaction",
    { ...transaction, actions: await packActions(baseUrl, transaction.actions) }
  );
}

export async function packSignedTransaction(baseUrl, signedTransaction) {
  if (typeof signedTransaction.transaction !== "string")
    signedTransaction = {
      ...signedTransaction,
      transaction: uint8ArrayToHex(
        new Uint8Array(
          await packTransaction(baseUrl, signedTransaction.transaction)
        )
      ),
    };
  return await postJsonGetArrayBuffer(
    baseUrl.replace(/\/+$/, "") + "/common/pack/SignedTransaction",
    signedTransaction
  );
}

export async function pushPackedSignedTransaction(baseUrl, packed) {
  const trace = await postArrayBufferGetJson(
    baseUrl.replace(/\/+$/, "") + "/native/push_transaction",
    packed
  );
  if (trace.error) throw new RPCError(trace.error, trace);
  return trace;
}

export async function packAndPushSignedTransaction(baseUrl, signedTransaction) {
  return await pushPackedSignedTransaction(
    baseUrl,
    await packSignedTransaction(baseUrl, signedTransaction)
  );
}

export async function signTransaction(baseUrl, transaction, privateKeys) {
  const keys = (privateKeys || []).map((k) => {
    if (typeof k === "string") return privateStringToKeyPair(k);
    else return k;
  });
  const claims = keys.map((k) => ({
    contract: "verifyec-sys",
    rawData: uint8ArrayToHex(publicKeyPairToFracpack(k)),
  }));
  transaction = new Uint8Array(
    await packTransaction(baseUrl, { ...transaction, claims })
  );
  const digest = new hashJs.sha256().update(transaction).digest();
  const proofs = keys.map((k) =>
    uint8ArrayToHex(
      signatureToFracpack({
        keyType: k.keyType,
        signature: k.keyPair.sign(digest),
      })
    )
  );
  return { transaction: uint8ArrayToHex(transaction), proofs };
}

export async function signAndPushTransaction(
  baseUrl,
  transaction,
  privateKeys
) {
  return await packAndPushSignedTransaction(
    baseUrl,
    await signTransaction(baseUrl, transaction, privateKeys)
  );
}

export function uint8ArrayToHex(data) {
  let result = "";
  for (const x of data) result += ("00" + x.toString(16)).slice(-2);
  return result.toUpperCase();
}

export function hexToUint8Array(hex) {
  if (typeof hex !== "string")
    throw new Error("Expected string containing hex digits");
  if (hex.length % 2) throw new Error("Odd number of hex digits");
  const l = hex.length / 2;
  const result = new Uint8Array(l);
  for (let i = 0; i < l; ++i) {
    const x = parseInt(hex.substring(i * 2, i * 2 + 2), 16);
    if (Number.isNaN(x)) throw new Error("Expected hex string");
    result[i] = x;
  }
  return result;
}
